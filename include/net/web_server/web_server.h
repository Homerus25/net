#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>

#include "boost/asio/io_context.hpp"

#if defined(NET_TLS)
#include "boost/asio/ssl/context.hpp"
#endif

#include "net/web_server/websocket_session.h"
#include "aliases.h"

namespace net {

using ws_session_ptr = std::weak_ptr<websocket_session>;

struct web_server {
#if defined(NET_TLS)
  explicit web_server(boost::asio::io_context&, boost::asio::ssl::context&);
#else
  explicit web_server(boost::asio::io_context&);
#endif
  ~web_server();

  web_server(web_server&&) = default;
  web_server& operator=(web_server&&) = default;

  web_server(web_server const&) = delete;
  web_server& operator=(web_server const&) = delete;

  void init(std::string const& host, std::string const& port,
            boost::system::error_code& ec) const;
  void run() const;
  void stop() const;

  void set_timeout(std::chrono::nanoseconds const& timeout) const;
  void set_request_body_limit(std::uint64_t limit) const;
  void set_request_queue_limit(std::size_t limit) const;

  void on_http_request(http_req_cb_t) const;
  void on_ws_msg(ws_msg_cb_t) const;
  void on_ws_open(ws_open_cb_t) const;
  void on_ws_close(ws_close_cb_t) const;
  void on_upgrade_ok(ws_upgrade_ok_cb_t) const;

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace net
