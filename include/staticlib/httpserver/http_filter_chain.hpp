/*
 * Copyright 2015, alex at staticlibs.net
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
 * File:   http_filter_chain.hpp
 * Author: alex
 *
 * Created on September 22, 2015, 11:26 AM
 */

#ifndef STATICLIB_HTTPSERVER_HTTP_FILTER_CHAIN_HPP
#define	STATICLIB_HTTPSERVER_HTTP_FILTER_CHAIN_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

#include "staticlib/httpserver/noncopyable.hpp"
#include "staticlib/httpserver/http_server.hpp"

namespace staticlib { 
namespace httpserver {

/**
 * A number of filters that executes sequentially. Request handler is executed after
 * the last filter. Each filter can stop filter chain execution by not calling
 * 'filter_chain.do_filter(...)'. In that case filter itself should handle the request.
 * Filter must not write anything to response if it won't going to stop filter chain.
 */
class http_filter_chain : staticlib::httpserver::noncopyable {
private:  
    /**
     * A list of filters to apply sequentially
     */
    std::vector<std::reference_wrapper<http_server::request_filter_type>> filters;
    /**
     * Request handler to apply after the filters
     */
    http_server::request_handler_type request_handler;
    /**
     * Index of current filter
     */
    std::atomic_size_t idx;
    
public:
    /**
     * Constructor
     * 
     * @param filters list of filters to apply sequentially
     * @param request_handler request handler to apply after the filters
     */
    http_filter_chain(std::vector<std::reference_wrapper<http_server::request_filter_type>> filters, 
            http_server::request_handler_type request_handler);
    
    /**
     * Applies current filter and gives it the ability to continue filter chain
     * execution by calling this method from inside the filter, or to handle the
     * request and stop the filter chain
     * 
     * @param request request shared pointer
     * @param conn client connection shared pointer
     */
    void do_filter(http_request_ptr& request, tcp_connection_ptr& conn);

};

} // namespace
}

#endif	/* STATICLIB_HTTPSERVER_HTTP_FILTER_CHAIN_HPP */

