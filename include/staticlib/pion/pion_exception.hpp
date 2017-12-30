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
 * File:   pion_exception.hpp
 * Author: alex
 *
 * Created on September 24, 2015, 1:38 PM
 */

#ifndef STATICLIB_PION_PION_EXCEPTION_HPP
#define STATICLIB_PION_PION_EXCEPTION_HPP

#include <exception>
#include <string>

#include "staticlib/support/exception.hpp"

namespace staticlib { 
namespace pion {

/**
 * Base exception class for business exceptions in staticlib modules
 */
class pion_exception : public sl::support::exception {
public:
    /**
     * Default constructor
     */
    pion_exception() = default;

    /**
     * Constructor with message
     * 
     * @param msg error message
     */
    pion_exception(const std::string& msg) :
    sl::support::exception(msg) { }

};

} // namespace
}

#endif /* STATICLIB_PION_PION_EXCEPTION_HPP */

