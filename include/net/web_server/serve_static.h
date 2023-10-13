#pragma once

#include "boost/beast/core/string.hpp"

#include "net/web_server/aliases.h"

namespace net {

bool serve_static_file(boost::beast::string_view doc_root,
                       http_req_t const& req,
                       http_res_cb_t const& cb);

}
