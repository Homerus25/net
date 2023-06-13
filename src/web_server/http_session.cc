#include "net/web_server/http_session.h"

#include <memory>
#include <optional>
#include <utility>

#include "boost/beast/http.hpp"
#include "boost/beast/websocket/rfc6455.hpp"
#if defined(NET_TLS)
#include "boost/beast/ssl.hpp"
#endif

#include "net/web_server/fail.h"
#include "net/web_server/responses.h"
#include "net/web_server/web_server.h"
#include "net/web_server/websocket_session.h"

namespace net {

// Handles an HTTP server connection.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template <class Derived>
struct http_session {
  // Access the derived class, this is part of
  // the Curiously Recurring Template Pattern idiom.
  Derived& derived() { return static_cast<Derived&>(*this); }

  // Construct the session
  http_session(boost::beast::flat_buffer buffer,
               web_server_settings_ptr settings)
      : buffer_(std::move(buffer)),
        settings_(std::move(settings)) {}

  void do_read() {
    reset_parser();
    set_connection_timeout();

    // Read a request using the parser-oriented interface
    boost::beast::http::async_read(
        derived().stream(), buffer_, *parser_,
        boost::beast::bind_front_handler(&http_session::on_read,
                                         derived().shared_from_this()));
  }
  void set_connection_timeout() {
    boost::beast::get_lowest_layer(derived().stream())
        .expires_after(settings_->timeout_);
  }

  void reset_parser() {
    parser_.reset(new boost::beast::http::request_parser<boost::beast::http::string_body>());

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser_->body_limit(settings_->request_body_limit_);
  }

  bool call_ws_upgrade_ok_callback() {
    return !settings_->ws_upgrade_ok_ ||
        settings_->ws_upgrade_ok_(parser_->get());
  }

  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == boost::beast::http::error::end_of_stream) {
      return derived().do_eof();
    }

    if (ec) {
      return fail(ec, "read");
    }

    // See if it is a WebSocket Upgrade
    if (boost::beast::websocket::is_upgrade(parser_->get())) {
      if (call_ws_upgrade_ok_callback()) {
        upgrade_session_to_ws();
        return;
      } else {
        web_server::http_res_t error_response{not_found_response(parser_->release(), "No upgrade possible")};
        send_respond(error_response);
      }
    } else {
      handle_incoming_message();
    }
  }
  void send_respond(web_server::http_res_t& msg) {
    outgoingMsg = std::make_shared<web_server::http_res_t>(std::move(msg));

    auto sse = [this](auto& lambda_msg){
      boost::beast::http::async_write(
          this->derived().stream(), lambda_msg,
          boost::beast::bind_front_handler(
              &http_session<Derived>::on_write, this->derived().shared_from_this(),
              lambda_msg.need_eof()));
    };

    std::visit(sse, *outgoingMsg);
  }

  void handle_incoming_message() {
    if (settings_->http_req_cb_) {
      settings_->http_req_cb_(
          parser_->release(),
          [this](web_server::http_res_t&& res) {
            send_respond(res);
          },
          derived().is_ssl());
    }
    else {
      web_server::http_res_t error_response{not_found_response(parser_->release(), "No handler implemented")};
      send_respond(error_response);
    }
  }

  void upgrade_session_to_ws() {  // Disable the timeout.
    // The websocket::stream uses its own timeout settings.
    boost::beast::get_lowest_layer(derived().stream()).expires_never();

    // Create a websocket session, transferring ownership
    // of both the socket and the HTTP request.
    make_websocket_session(derived().release_stream(), parser_->release(),
                                  settings_);
  }

  void on_write(bool close, boost::beast::error_code ec,
                std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
      return fail(ec, "write");
    }

    if (close) {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return derived().do_eof();
    }

    do_read();
  }

  boost::beast::flat_buffer buffer_;
  std::unique_ptr<boost::beast::http::request_parser<boost::beast::http::string_body>> parser_;
  web_server_settings_ptr settings_;
  std::shared_ptr<web_server::http_res_t> outgoingMsg;
};

//------------------------------------------------------------------------------

// Handles a plain HTTP connection
struct plain_http_session
    : public http_session<plain_http_session>,
      public std::enable_shared_from_this<plain_http_session> {
  // Create the session
  plain_http_session(boost::beast::tcp_stream&& stream,
                     boost::beast::flat_buffer&& buffer,
                     web_server_settings_ptr settings)
      : http_session<plain_http_session>(std::move(buffer),
                                         std::move(settings)),
        stream_(std::move(stream)) {}

  // Start the session
  void run() { this->do_read(); }

  // Called by the base class
  boost::beast::tcp_stream& stream() { return stream_; }

  // Called by the base class
  boost::beast::tcp_stream release_stream() { return std::move(stream_); }

  // Called by the base class
  void do_eof() {
    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
  }

  static bool is_ssl() { return false; }

  boost::beast::tcp_stream stream_;
};

//------------------------------------------------------------------------------

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::beast::flat_buffer&& buffer,
                       web_server_settings_ptr const& settings) {
  std::make_shared<plain_http_session>(std::move(stream), std::move(buffer),
                                       settings)
      ->run();
}

//------------------------------------------------------------------------------

// Handles an SSL HTTP connection
#if defined(NET_TLS)
struct ssl_http_session
    : public http_session<ssl_http_session>,
      public std::enable_shared_from_this<ssl_http_session> {
  // Create the http_session
  ssl_http_session(boost::beast::tcp_stream&& stream,
                   boost::asio::ssl::context& ctx,
                   boost::beast::flat_buffer&& buffer,
                   web_server_settings_ptr settings)
      : http_session<ssl_http_session>(std::move(buffer), std::move(settings)),
        stream_(std::move(stream), ctx) {}

  // Start the session
  void run() {
    // Set the timeout.
    boost::beast::get_lowest_layer(stream_).expires_after(settings_->timeout_);

    // Perform the SSL handshake
    // Note, this is the buffered version of the handshake.
    stream_.async_handshake(
        boost::asio::ssl::stream_base::server, buffer_.data(),
        boost::beast::bind_front_handler(&ssl_http_session::on_handshake,
                                         shared_from_this()));
  }

  // Called by the base class
  boost::beast::ssl_stream<boost::beast::tcp_stream>& stream() {
    return stream_;
  }

  // Called by the base class
  boost::beast::ssl_stream<boost::beast::tcp_stream> release_stream() {
    return std::move(stream_);
  }

  // Called by the base class
  void do_eof() {
    // Set the timeout.
    boost::beast::get_lowest_layer(stream_).expires_after(settings_->timeout_);

    // Perform the SSL shutdown
    stream_.async_shutdown(ssl_http_session::on_shutdown);
  }

  static bool is_ssl() { return true; }

private:
  void on_handshake(boost::beast::error_code ec, std::size_t bytes_used) {
    if (ec) {
      return fail(ec, "handshake");
    }

    // Consume the portion of the buffer used by the handshake
    buffer_.consume(bytes_used);

    do_read();
  }

  static void on_shutdown(boost::beast::error_code ec) {
    if (ec) {
      return fail(ec, "shutdown");
    }

    // At this point the connection is closed gracefully
  }

  boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
};

//------------------------------------------------------------------------------

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::asio::ssl::context& ctx,
                       boost::beast::flat_buffer&& buffer,
                       web_server_settings_ptr const& settings) {
  std::make_shared<ssl_http_session>(std::move(stream), ctx, std::move(buffer),
                                     settings)
      ->run();
}
#endif

}  // namespace net
