#pragma once

#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/file_body.hpp>

namespace net {
struct websocket_session;

enum class ws_msg_type { TEXT, BINARY };

using http_req_t =
    boost::beast::http::request<boost::beast::http::string_body>;
using string_res_t =
    boost::beast::http::response<boost::beast::http::string_body>;
using buffer_res_t =
    boost::beast::http::response<boost::beast::http::buffer_body>;
using file_res_t =
    boost::beast::http::response<boost::beast::http::file_body>;
using empty_res_t =
    boost::beast::http::response<boost::beast::http::empty_body>;
using http_res_t =
    std::variant<string_res_t, buffer_res_t, file_res_t, empty_res_t>;

using http_res_cb_t = std::function<void(http_res_t&&)>;
using http_req_cb_t = std::function<void(http_req_t, http_res_cb_t, bool)>;

using ws_session_ptr = std::weak_ptr<websocket_session>;

using ws_msg_cb_t =
    std::function<void(ws_session_ptr, std::string const&, ws_msg_type)>;
using ws_open_cb_t = std::function<void(
    ws_session_ptr, std::string const& /* target */, bool /* is SSL */)>;
using ws_close_cb_t = std::function<void(void*)>;
using ws_upgrade_ok_cb_t = std::function<bool(http_req_t const&)>;
}