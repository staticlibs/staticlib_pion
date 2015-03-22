// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <pion/algorithm.hpp>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <climits>
#include <cerrno>
#include <cctype>

namespace pion {        // begin namespace pion

// http://stackoverflow.com/a/27813
bool algorithm::iequals(const std::string& str1, const std::string& str2) {
    if (str1.size() != str2.size()) {
        return false;
    }
    for (std::string::const_iterator c1 = str1.begin(), c2 = str2.begin(); c1 != str1.end(); ++c1, ++c2) {
        if (std::tolower(*c1) != std::tolower(*c2)) {
            return false;
        }
    }
    return true;
}

size_t algorithm::parse_sizet(const std::string& str) {
    auto cstr = str.c_str();
    char* endptr;
    errno = 0;
    auto l = strtoull(cstr, &endptr, 0);
    if (errno == ERANGE || cstr + str.length() != endptr) {
        throw std::runtime_error(str);
    }
    return static_cast<size_t> (l);
}

uint16_t algorithm::parse_uint16(const std::string& str) {
    auto cstr = str.c_str();
    char* endptr;
    errno = 0;
    auto l = strtol(cstr, &endptr, 0);
    if (errno == ERANGE || cstr + str.length() != endptr) {
        throw std::runtime_error(str);
    }
    if (l < 0 || l > USHRT_MAX) {
        throw std::runtime_error(str);
    }
    return static_cast<uint16_t> (l);
}

// http://stackoverflow.com/a/17976541
std::string algorithm::trim(const std::string& s) {
    auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c) {
        return std::isspace(c);
    });
    return std::string(wsfront, std::find_if_not(s.rbegin(), std::string::const_reverse_iterator(wsfront), [](int c) {
        return std::isspace(c);
    }).base());
}

std::string algorithm::url_decode(const std::string& str)
{
    char decode_buf[3];
    std::string result;
    result.reserve(str.size());
    
    for (std::string::size_type pos = 0; pos < str.size(); ++pos) {
        switch(str[pos]) {
        case '+':
            // convert to space character
            result += ' ';
            break;
        case '%':
            // decode hexadecimal value
            if (pos + 2 < str.size()) {
                decode_buf[0] = str[++pos];
                decode_buf[1] = str[++pos];
                decode_buf[2] = '\0';

                char decoded_char = static_cast<char>( strtol(decode_buf, 0, 16) );

                // decoded_char will be '\0' if strtol can't parse decode_buf as hex
                // (or if decode_buf == "00", which is also not valid).
                // In this case, recover from error by not decoding.
                if (decoded_char == '\0') {
                    result += '%';
                    pos -= 2;
                } else
                    result += decoded_char;
            } else {
                // recover from error by not decoding character
                result += '%';
            }
            break;
        default:
            // character does not need to be escaped
            result += str[pos];
        }
    };
    
    return result;
}
    
std::string algorithm::url_encode(const std::string& str)
{
    char encode_buf[4];
    std::string result;
    encode_buf[0] = '%';
    result.reserve(str.size());

    // character selection for this algorithm is based on the following url:
    // http://www.blooberry.com/indexdot/html/topics/urlencoding.htm
    
    for (std::string::size_type pos = 0; pos < str.size(); ++pos) {
        switch(str[pos]) {
        default:
            if (str[pos] > 32 && str[pos] < 127) {
                // character does not need to be escaped
                result += str[pos];
                break;
            }
            // else pass through to next case
        case ' ':   
        case '$': case '&': case '+': case ',': case '/': case ':':
        case ';': case '=': case '?': case '@': case '"': case '<':
        case '>': case '#': case '%': case '{': case '}': case '|':
        case '\\': case '^': case '~': case '[': case ']': case '`':
            // the character needs to be encoded
            sprintf(encode_buf+1, "%.2X", (unsigned char)(str[pos]));
            result += encode_buf;
            break;
        }
    };
    
    return result;
}

// TODO
//std::string algorithm::xml_decode(const std::string& str)
//{
//}

std::string algorithm::xml_encode(const std::string& str)
{
    std::string result;
    result.reserve(str.size() + 20);    // Assume ~5 characters converted (length increases)
    const unsigned char *ptr = reinterpret_cast<const unsigned char*>(str.c_str());
    const unsigned char *end_ptr = ptr + str.size();
    while (ptr < end_ptr) {
        // check byte ranges for valid UTF-8
        // see http://en.wikipedia.org/wiki/UTF-8
        // also, see http://www.w3.org/TR/REC-xml/#charsets
        // this implementation is the strictest subset of both
        if ((*ptr >= 0x20 && *ptr <= 0x7F) || *ptr == 0x9 || *ptr == 0xa || *ptr == 0xd) {
            // regular ASCII character
            switch(*ptr) {
                    // Escape special XML characters.
                case '&':
                    result += "&amp;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '\"':
                    result += "&quot;";
                    break;
                case '\'':
                    result += "&apos;";
                    break;
                default:
                    result += *ptr;
            }
        } else if (*ptr >= 0xC2 && *ptr <= 0xDF) {
            // two-byte sequence
            if (*(ptr+1) >= 0x80 && *(ptr+1) <= 0xBF) {
                result += *ptr;
                result += *(++ptr);
            } else {
                // insert replacement char
#ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable: 4309 )
#endif // _MSC_VER                
                result += 0xef;
                result += 0xbf;
                result += 0xbd;
#ifdef _MSC_VER
    #pragma warning( pop )
#endif // _MSC_VER                
            }
        } else if (*ptr >= 0xE0 && *ptr <= 0xEF) {
            // three-byte sequence
            if (*(ptr+1) >= 0x80 && *(ptr+1) <= 0xBF
                && *(ptr+2) >= 0x80 && *(ptr+2) <= 0xBF) {
                result += *ptr;
                result += *(++ptr);
                result += *(++ptr);
            } else {
                // insert replacement char
#ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable: 4309 )
#endif // _MSC_VER                
                result += 0xef;
                result += 0xbf;
                result += 0xbd;
#ifdef _MSC_VER
    #pragma warning( pop )
#endif // _MSC_VER                
            }
        } else if (*ptr >= 0xF0 && *ptr <= 0xF4) {
            // four-byte sequence
            if (*(ptr+1) >= 0x80 && *(ptr+1) <= 0xBF
                && *(ptr+2) >= 0x80 && *(ptr+2) <= 0xBF
                && *(ptr+3) >= 0x80 && *(ptr+3) <= 0xBF) {
                result += *ptr;
                result += *(++ptr);
                result += *(++ptr);
                result += *(++ptr);
            } else {
                // insert replacement char
#ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable: 4309 )
#endif // _MSC_VER                
                result += 0xef;
                result += 0xbf;
                result += 0xbd;
#ifdef _MSC_VER
    #pragma warning( pop )
#endif // _MSC_VER                
            }
        } else {
            // insert replacement char
#ifdef _MSC_VER
    #pragma warning( push )
    #pragma warning( disable: 4309 )
#endif // _MSC_VER                
            result += 0xef;
            result += 0xbf;
            result += 0xbd;
#ifdef _MSC_VER
    #pragma warning( pop )
#endif // _MSC_VER                
        }
        ++ptr;
    }
    
    return result;
}
    
}   // end namespace pion
