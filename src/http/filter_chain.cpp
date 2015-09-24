/* 
 * File:   filter_chain.cpp
 * Author: alex
 * 
 * Created on September 22, 2015, 11:26 AM
 */

#include <atomic>
#include <vector>
#include <cstdint>

#include "pion/http/filter_chain.hpp"

namespace pion {
namespace http {

filter_chain::filter_chain(std::vector<std::reference_wrapper<streaming_server::request_filter_type>> filters, 
        streaming_server::request_handler_type request_handler) :
filters(std::move(filters)),
request_handler(std::move(request_handler)),
idx(0) { }

void filter_chain::do_filter(http::request_ptr& request, tcp::connection_ptr& conn) {
    size_t cur_idx = idx.fetch_add(1);
    if (cur_idx < filters.size()) {
        filters[cur_idx].get()(request, conn, *this);
        return;
    }
    request_handler(request, conn);
}

} // end namespace http
} // end namespace pion

