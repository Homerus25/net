#pragma once

#include "aliases.h"

namespace net {

struct web_server_settings {
  http_req_cb_t http_req_cb_;
  ws_msg_cb_t ws_msg_cb_;
  ws_open_cb_t ws_open_cb_;
  ws_close_cb_t ws_close_cb_;
  ws_upgrade_ok_cb_t ws_upgrade_ok_;

  std::chrono::nanoseconds timeout_{std::chrono::seconds(60)};
  std::uint64_t request_body_limit_{1024 * 1024};
  std::size_t request_queue_limit_{8};
};

using web_server_settings_ptr = std::shared_ptr<web_server_settings>;

}  // namespace net
