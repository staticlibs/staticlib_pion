/*
 * Copyright 2016, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * File:   http_filter_chain.cpp
 * Author: alex
 * 
 * Created on September 22, 2015, 11:26 AM
 */

#include "staticlib/pion/http_filter_chain.hpp"

namespace staticlib { 
namespace pion {

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
