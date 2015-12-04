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
 * File:   filter_chain.hpp
 * Author: alex
 *
 * Created on September 22, 2015, 11:26 AM
 */

#ifndef __PION_HTTP_FILTER_CHAIN_HPP__
#define	__PION_HTTP_FILTER_CHAIN_HPP__

#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

#include "pion/noncopyable.hpp"
#include "pion/http/streaming_server.hpp"

namespace pion {
namespace http {

/**
 * A number of filters that executes sequentially. Request handler is executed after
 * the last filter. Each filter can stop filter chain execution by not calling
 * 'filter_chain.do_filter(...)'. In that case filter itself should handle the request.
 * Filter must not write anything to response if it won't going to stop filter chain.
 */
class filter_chain : pion::noncopyable {
private:  
    /**
     * A list of filters to apply sequentially
     */
    std::vector<std::reference_wrapper<streaming_server::request_filter_type>> filters;
    /**
     * Request handler to apply after the filters
     */
    streaming_server::request_handler_type request_handler;
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
    filter_chain(std::vector<std::reference_wrapper<streaming_server::request_filter_type>> filters, 
            streaming_server::request_handler_type request_handler);
    
    /**
     * Applies current filter and gives it the ability to continue filter chain
     * execution by calling this method from inside the filter, or to handle the
     * request and stop the filter chain
     * 
     * @param request request shared pointer
     * @param conn client connection shared pointer
     */
    void do_filter(http::request_ptr& request, tcp::connection_ptr& conn);

};

} // end namespace http
} // end namespace pion

#endif	/* __PION_HTTP_FILTER_CHAIN_HPP__ */

