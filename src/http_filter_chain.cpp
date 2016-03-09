/* 
 * File:   http_filter_chain.cpp
 * Author: alex
 * 
 * Created on September 22, 2015, 11:26 AM
 */

#include "staticlib/httpserver/http_filter_chain.hpp"

namespace staticlib { 
namespace httpserver {

http_filter_chain::http_filter_chain(std::vector<std::reference_wrapper<http_server::request_filter_type>> filters, 
        http_server::request_handler_type request_handler) :
filters(std::move(filters)),
request_handler(std::move(request_handler)),
idx(0) { }

void http_filter_chain::do_filter(http_request_ptr& request, tcp_connection_ptr& conn) {
    size_t cur_idx = idx.fetch_add(1);
    if (cur_idx < filters.size()) {
        filters[cur_idx].get()(request, conn, *this);
        return;
    }
    request_handler(request, conn);
}

} // namespace
}
