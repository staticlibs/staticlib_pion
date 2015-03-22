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
#include <pion/config.hpp>

namespace pion {    // begin namespace pion

struct PION_API algorithm {
    
    // http://stackoverflow.com/a/2595226
    template <class T>
    static void hash_combine(std::size_t& seed, const T& v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    // http://stackoverflow.com/a/27813
    static bool iequals(const std::string& str1, const std::string& str2);
    
    // std::to_string is not available in some GCC versions
    template<typename T>
    static std::string to_string(T t) {
        std::stringstream ss{};
        ss << t;
        return ss.str();
    }
    
    static size_t parse_sizet(const std::string& str);
    
    static uint16_t parse_uint16(const std::string& str);
    
    static std::string trim(const std::string& s);
    
    /// escapes URL-encoded strings (a%20value+with%20spaces)
    static std::string url_decode(const std::string& str);

    /// encodes strings so that they are safe for URLs (with%20spaces)
    static std::string url_encode(const std::string& str);

    /// TODO: escapes XML/HTML-encoded strings (1 &lt; 2)
    //static std::string xml_decode(const std::string& str);
    
    /// encodes strings so that they are safe for XML/HTML (2 &gt; 1)
    static std::string xml_encode(const std::string& str);
    
};
    
}   // end namespace pion

#endif
