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
 * File:   httpserver_exception.hpp
 * Author: alex
 *
 * Created on September 24, 2015, 1:38 PM
 */

#ifndef STATICLIB_HTTPSERVER_HTTPSERVER_EXCEPTION_HPP
#define	STATICLIB_HTTPSERVER_HTTPSERVER_EXCEPTION_HPP

#include <exception>
#include <string>

#include "staticlib/support/exception.hpp"

namespace staticlib { 
namespace httpserver {

/**
 * Base exception class for business exceptions in staticlib modules
 */
class httpserver_exception : public sl::support::exception {
public:
    /**
     * Default constructor
     */
    httpserver_exception() = default;

    /**
     * Constructor with message
     * 
     * @param msg error message
     */
    httpserver_exception(const std::string& msg) :
    sl::support::exception(msg) { }

};

} // namespace
}

#endif	/* STATICLIB_HTTPSERVER_HTTPSERVER_EXCEPTION_HPP */

