#pragma once

#include <string_view>

#include "boost/beast/core/string_type.hpp"

#include "aliases.h"

namespace net {

enum class http_content_encoding { IDENTITY, GZIP };

http_content_encoding select_content_encoding(
    boost::beast::string_view accept_encoding);

void set_response_body(string_res_t& res,
                       http_content_encoding encoding,
                       std::string_view content);

void set_response_body(string_res_t& res,
                       http_req_t const& req,
                       std::string_view content);

}  // namespace net
