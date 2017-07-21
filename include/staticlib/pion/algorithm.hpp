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

#ifndef STATICLIB_PION_ALGORITHM_HPP
#define STATICLIB_PION_ALGORITHM_HPP

#include <cctype>
#include <cstdint>
#include <functional>
#include <locale>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/utils.hpp"

namespace staticlib { 
namespace pion {

/**
 * Contains some common algorithms
 * 
 */
namespace algorithm {

    /**
     * Detect char
     * 
     * @param c integer
     * @return true if char, false otherwise
     */
    inline bool is_char(int c) {
        return (c >= 0 && c <= 127);
    }

    /**
     * Detect control char
     * 
     * @param c integer
     * @return true if control char, false otherwise
     */
    inline bool is_control(int c) {
        return ( (c >= 0 && c <= 31) || c == 127);
    }

    /**
     * Detect special char
     * 
     * @param c integer
     * @return true if special char, false otherwise
     */
    inline bool is_special(int c) {
        switch (c) {
        case '(': case ')': case '<': case '>': case '@':
        case ',': case ';': case ':': case '\\': case '"':
        case '/': case '[': case ']': case '?': case '=':
        case '{': case '}': case ' ': case '\t':
            return true;
        default:
            return false;
        }
    }

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
        bool operator()(std::string const& x, std::string const& y) const {
            return sl::utils::iequals(x, y);
        }
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
        std::size_t operator()(std::string const& x) const {
            std::size_t seed = 0;
            for (std::string::const_iterator it = x.begin(); it != x.end(); ++it) {
                // locale is expensive here
                algorithm::hash_combine(seed, std::toupper(*it/*, locale*/));
            }
            return seed;
        }
    };
    
} // namespace
}
}

#endif // STATICLIB_PION_ALGORITHM_HPP
