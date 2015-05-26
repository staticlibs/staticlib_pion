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

// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_ALGORITHM_HEADER__
#define __PION_ALGORITHM_HEADER__

#include <string>
#include <functional>
#include <cstdint>
#include <sstream>

#include "pion/config.hpp"

namespace pion {

/**
 * Contains some common algorithms
 * 
 */
namespace algorithm {
    
    /**
     * Called repeatedly to incrementally create a hash value from several variables.
     * 
     * @see http://www.boost.org/doc/libs/1_58_0/doc/html/hash/combine.html
     * @param seed seed value for this hash
     * @param v value to compute hash over
     */
    // http://stackoverflow.com/a/2595226
    template <class T>
    static void hash_combine(std::size_t& seed, const T& v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    
    /**
     * Implementation of C++11 "std::to_string" function for the compilers
     * that do not support it. Uses "std::stringstream".
     * 
     * @param t value to stringify
     * @return string representation of specified value
     */
    template<typename T>
    static std::string to_string(T t) {
        std::stringstream ss{};
        ss << t;
        return ss.str();
    }

    /**
     * Case insensitive byte-to-byte string comparison, does not support Unicode
     * 
     * @param str1 first string
     * @param str2 seconds string
     * @return true if strings equal ignoring case, false otherwise
     */
    // http://stackoverflow.com/a/27813
    bool iequals(const std::string& str1, const std::string& str2);
    
    /**
     * Parse "size_t" integer from specified string
     * 
     * @param str string containing integer
     * @return parsed value
     * @throws std::runtime_error on parse error
     */
    size_t parse_sizet(const std::string& str);

    /**
     * Parse "uint16_t" integer from specified string
     * 
     * @param str string containing integer
     * @return parsed value
     * @throws std::runtime_error on parse error
     */
    uint16_t parse_uint16(const std::string& str);
    
    /**
     * Trims specified string from left and from right using "std::isspace"
     * to check empty bytes, does not support Unicode
     * 
     * @param s string to trim
     * @return trimmed string
     */
    std::string trim(const std::string& s);
    
    /**
     * Unescapes specified URL-encoded string (a%20value+with%20spaces)
     * 
     * @param str URL-encoded string
     * @return unescaped (plain) string
     */
    std::string url_decode(const std::string& str);

    /**
     * Encodes specified string so that it is safe for URLs (with%20spaces)
     * 
     * @param str string to encode
     * @return escaped string
     */
    std::string url_encode(const std::string& str);

    /**
     * Escapes specified string so it is safe to use inside XML/HTML (2 &gt; 1)
     * 
     * @param str string to escape
     * @return escaped string
     */
    std::string xml_encode(const std::string& str);
    

    /**
     * Case insensitive string equality predicate
     */
    // http://www.boost.org/doc/libs/1_50_0/doc/html/unordered/hash_equality.html
    struct iequal_to : std::binary_function<std::string, std::string, bool> {
        /**
         * Case insensitive byte-to-byte string comparison, does not support Unicode
         * 
         * @param x first string
         * @param y seconds string
         * @return true if strings equal ignoring case, false otherwise
         */
        bool operator()(std::string const& x, std::string const& y) const;
    };

    /**
     * Case insensitive hash generic function
     */
    // http://www.boost.org/doc/libs/1_50_0/doc/html/unordered/hash_equality.html
    struct ihash : std::unary_function<std::string, std::size_t> {
        /**
         * Computes hash over specified string
         * 
         * @param x string to compute hash on
         * @return hash value
         */
        std::size_t operator()(std::string const& x) const;
    };
    
} // end namespace algorithm
} // end namespace pion

#endif // __PION_ALGORITHM_HEADER__
