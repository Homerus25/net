#pragma once

#include "boost/beast/http.hpp"
#include "boost/beast/websocket/rfc6455.hpp"

#include <memory>

template<typename http_session, typename response>
struct pending_request {
  explicit pending_request(http_session& session) : self_(session) {}

  bool is_finished() const { return static_cast<bool>(response_); }

  // Called by the HTTP handler to send a response.
  template <bool IsRequest, class Body, class Fields>
  void operator()(
      boost::beast::http::message<IsRequest, Body, Fields>&& msg) {
    // This holds a work item
    struct response_impl : response {
      http_session& self_;
      boost::beast::http::message<IsRequest, Body, Fields> msg_;

      response_impl(
          http_session& self,
          boost::beast::http::message<IsRequest, Body, Fields>&& msg)
          : self_(self), msg_(std::move(msg)) {}

      void send() override {
        boost::beast::http::async_write(
            self_.derived().stream(), msg_,
            boost::beast::bind_front_handler(
                &http_session::on_write, self_.derived().shared_from_this(),
                msg_.need_eof()));
      }
    };

    response_ = std::make_unique<response_impl>(self_, std::move(msg));
    boost::asio::post(self_.derived().stream().get_executor(),
                      [&, self = self_.derived().shared_from_this()]() {
                        self_.send_next_response();
                      });
  }

  http_session& self_;
  std::unique_ptr<response> response_;
};