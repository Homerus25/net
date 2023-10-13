#pragma once

#include <string>
#include <string_view>

#include "boost/beast/http/status.hpp"

#include "aliases.h"

namespace net {

string_res_t string_response(
    http_req_t const& req, std::string_view text,
    boost::beast::http::status status = boost::beast::http::status::ok,
    std::string_view content_type = "text/html");

string_res_t not_found_response(
    http_req_t const& req, std::string_view text = "Not found",
    std::string_view content_type = "text/html");

string_res_t server_error_response(
    http_req_t const& req,
    std::string_view text = "Internal server error",
    std::string_view content_type = "text/html");

string_res_t bad_request_response(
    http_req_t const& req, std::string_view text = "Bad request",
    std::string_view content_type = "text/html");

empty_res_t empty_response(
    http_req_t const& req,
    boost::beast::http::status status = boost::beast::http::status::ok,
    std::string_view content_type = "text/html");

string_res_t moved_response(
    http_req_t const& req, std::string_view new_location,
    boost::beast::http::status status =
        boost::beast::http::status::moved_permanently,
    std::string_view content_type = "text/html");

}  // namespace net
