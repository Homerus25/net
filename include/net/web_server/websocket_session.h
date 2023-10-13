#pragma once

#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/string_body.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket.hpp>
#include <queue>

#if defined(NET_TLS)
#include "boost/beast/ssl.hpp"
#endif

#include "net/web_server/web_server_settings.h"

namespace net {

void make_websocket_session(
    boost::beast::tcp_stream stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server_settings_ptr const& settings);

#if defined(NET_TLS)
void make_websocket_session(
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server_settings_ptr const& settings);
#endif


struct websocket_session : public std::enable_shared_from_this<websocket_session> {
  using send_cb_t = std::function<void(boost::system::error_code, std::size_t)>;

  explicit websocket_session(boost::beast::tcp_stream&& stream, web_server_settings_ptr settings)
      : settings_(std::move(settings)), ws_(std::move(stream)) {}

  ~websocket_session();

  websocket_session(websocket_session const&) = delete;
  websocket_session& operator=(websocket_session const&) = delete;
  websocket_session(websocket_session&&) = delete;
  websocket_session& operator=(websocket_session&&) = delete;

  void run(boost::beast::http::request<boost::beast::http::string_body>&& req);
  void on_close(std::function<void()>&& fn);
  void on_msg(std::function<void(std::string const&, ws_msg_type)>&& fn);
  void send(std::vector<std::byte> msg, ws_msg_type type, send_cb_t cb);

private:
  void do_accept(boost::beast::http::request<boost::beast::http::string_body> req);
  void on_accept(std::string const& target, boost::beast::error_code ec);
  void do_read();
  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
  void on_write(boost::beast::error_code ec, std::size_t bytes_transferred);
  void send_next();

  boost::beast::flat_buffer buffer_;
  web_server_settings_ptr settings_;

  std::queue<std::tuple<std::vector<std::byte>, ws_msg_type, send_cb_t>> send_queue_;
  bool send_active_{false};

  std::function<void()> on_close_;
  std::function<void(std::string const&, ws_msg_type)> on_msg_;
  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
};

}  // namespace net
