#include "net/web_server/responses.h"

#include "boost/beast/version.hpp"

#include "net/web_server/content_encoding.h"

namespace http = boost::beast::http;

namespace net {

string_res_t string_response(http_req_t const& req,
                                         std::string_view const text,
                                         http::status status,
                                         std::string_view const content_type) {
  string_res_t res{status, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, content_type);
  res.keep_alive(req.keep_alive());
  set_response_body(res, req, text);
  res.prepare_payload();
  return res;
}

string_res_t not_found_response(
    http_req_t const& req, std::string_view const text,
    std::string_view const content_type) {
  return string_response(req, text, http::status::not_found, content_type);
}

string_res_t server_error_response(
    http_req_t const& req, std::string_view const text,
    std::string_view const content_type) {
  return string_response(req, text, http::status::internal_server_error,
                         content_type);
}

string_res_t bad_request_response(
    http_req_t const& req, std::string_view const text,
    std::string_view const content_type) {
  return string_response(req, text, http::status::bad_request, content_type);
}

empty_res_t empty_response(http_req_t const& req,
                                       boost::beast::http::status status,
                                       std::string_view const content_type) {
  empty_res_t res{status, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, content_type);
  res.keep_alive(req.keep_alive());
  return res;
}

string_res_t moved_response(http_req_t const& req,
                                        std::string_view new_location,
                                        boost::beast::http::status status,
                                        std::string_view content_type) {
  string_res_t res{status, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, content_type);
  res.set(http::field::location, new_location);
  res.keep_alive(req.keep_alive());
  res.body() = http::obsolete_reason(status);
  res.prepare_payload();
  return res;
}

}  // namespace net
