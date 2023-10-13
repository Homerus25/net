#pragma once

#include <regex>
#include <string>
#include <vector>

#include "net/web_server/aliases.h"

namespace net {

struct query_router {
  using request = http_req_t;
  using reply = http_res_t;

  struct route_request : public http_req_t {
    route_request(request req) : http_req_t(req) {}
    std::vector<std::string> path_params_;
    std::string username_, password_;
  };

  using route_request_handler = std::function<void(
      route_request const&, http_res_cb_t, bool)>;

  query_router& route(std::string method, std::string const& path_regex,
                      route_request_handler handler);
  void operator()(http_req_t, http_res_cb_t const&,
                  bool);
  void reply_hook(std::function<void(http_res_t&)> reply_hook);
  void enable_cors();

private:
  struct handler {
    std::string method_;
    std::regex path_;
    route_request_handler request_handler_;
  };

  static void decode_content(request& req);

  static void set_credentials(route_request& req);

  std::vector<handler> routes_;
  std::function<void(reply&)> reply_hook_;
};

}  // namespace net