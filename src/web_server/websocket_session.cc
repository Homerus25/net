#include "net/web_server/websocket_session.h"

#include <memory>
#include <queue>
#include <utility>

#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/version.hpp"
#include "boost/beast/websocket.hpp"

#include "net/web_server/fail.h"

namespace net {

void websocket_session::run(
    boost::beast::http::request<boost::beast::http::string_body>&& req) {
  // Accept the WebSocket upgrade request
  do_accept(std::move(req));
}

void websocket_session::on_read(boost::beast::error_code ec,
                                std::size_t bytes_transferred) {
  //read_active_ = false;
  boost::ignore_unused(bytes_transferred);

  // This indicates that the websocket_session was closed
  if (ec == boost::beast::websocket::error::closed) {
    return;
  }

  if (ec) {
    return fail(ec, "read");
  }

  if (on_msg_) {
    on_msg_(
        boost::beast::buffers_to_string(buffer_.data()),
        ws_.got_text() ? ws_msg_type::TEXT : ws_msg_type::BINARY);
  } else if (settings_->ws_msg_cb_) {
    settings_->ws_msg_cb_(
        shared_from_this(),
        boost::beast::buffers_to_string(buffer_.data()),
        ws_.got_text() ? ws_msg_type::TEXT : ws_msg_type::BINARY);
  }

  buffer_.consume(buffer_.size());
  do_read();
}

void websocket_session::do_read() {
  // Read a message into our buffer
  ws_.async_read(buffer_, boost::beast::bind_front_handler(
                              &websocket_session::on_read,
                              shared_from_this()));
}

void websocket_session::do_accept(
    boost::beast::http::request<boost::beast::http::string_body> req) {
  // Set suggested timeout settings for the websocket
  ws_.set_option(
      boost::beast::websocket::stream_base::timeout::suggested(
          boost::beast::role_type::server));

  // Set a decorator to change the Server of the handshake
  ws_.set_option(boost::beast::websocket::stream_base::decorator(
      [](boost::beast::websocket::response_type& res) {
        res.set(boost::beast::http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING));
      }));

  // Accept the websocket handshake
  ws_.async_accept(
      req, boost::beast::bind_front_handler(&websocket_session::on_accept,
                                            shared_from_this(),
                                            std::string{req.target()}));
}

void websocket_session::on_accept(const std::string& target,
                                  boost::beast::error_code ec) {
  if (ec) {
    return fail(ec, "accept");
  }

  if (settings_->ws_open_cb_) {
    boost::asio::post(ws_.get_executor(),
                      [&, self = shared_from_this(), target] {
                        settings_->ws_open_cb_(self, target, false); //is_ssl()
                      });
  }

  // Read a message
  do_read();
}

websocket_session::~websocket_session() {
  if (on_close_) {
    on_close_();
  } else if (settings_->ws_close_cb_) {
    settings_->ws_close_cb_(this);
  }
}

void websocket_session::on_close(std::function<void()>&& fn) {
  on_close_ = std::move(fn);
}

void websocket_session::on_msg(
    std::function<void(const std::string&, ws_msg_type)>&& fn) {
  on_msg_ = std::move(fn);
}

void websocket_session::send(std::vector<std::byte> msg, ws_msg_type type,
                             websocket_session::send_cb_t cb) {
  send_queue_.emplace(std::move(msg), type, cb);
  send_next();
}
void websocket_session::send_next() {
  if (send_active_ || send_queue_.empty()) {
    return;
  }

  std::vector<std::byte> msg;
  ws_msg_type type;  // NOLINT
  send_cb_t cb;
  std::tie(msg, type, cb) = send_queue_.front();
  send_queue_.pop();
  send_active_ = true;

  auto m = std::make_shared<std::vector<std::byte>>(std::move(msg));
  ws_.text(type == ws_msg_type::TEXT);
  ws_.async_write(
      boost::asio::buffer(m->data(), m->size()),
      [m, cb, self = shared_from_this()](
          boost::system::error_code const& ec,
          std::size_t bytes_transferred) {
        self->send_active_ = false;
        self->send_next();
        boost::asio::post(
            self->ws_.get_executor(),
            [cb, ec, bytes_transferred]() { cb(ec, bytes_transferred); });
      });
}

void websocket_session::on_write(boost::beast::error_code ec,
                                 std::size_t bytes_transferred) {
  boost::ignore_unused(bytes_transferred);

  if (ec) {
    return fail(ec, "write");
  }

  // Clear the buffer
  buffer_.consume(buffer_.size());
}

//------------------------------------------------------------------------------

void make_websocket_session(
    boost::beast::tcp_stream stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server_settings_ptr const& settings) {
  std::make_shared<websocket_session>(std::move(stream), settings)
      ->run(std::move(req));
}

//------------------------------------------------------------------------------

// Handles an SSL WebSocket connection
#if defined(NET_TLS)
struct ssl_websocket_session
    : public websocket_session<ssl_websocket_session>,
      public std::enable_shared_from_this<ssl_websocket_session> {
  // Create the ssl_websocket_session
  explicit ssl_websocket_session(
      boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
      web_server_settings_ptr settings)
      : websocket_session<ssl_websocket_session>(std::move(settings)),
        ws_(std::move(stream)) {}

  // Called by the base class
  boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::beast::tcp_stream>>&
  ws() {
    return ws_;
  }

  static bool is_ssl() { return true; }

private:
  boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::beast::tcp_stream>>
      ws_;
};

void make_websocket_session(
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server_settings_ptr const& settings) {
  std::make_shared<ssl_websocket_session>(std::move(stream), settings)
      ->run(std::move(req));
}
#endif

}  // namespace net
