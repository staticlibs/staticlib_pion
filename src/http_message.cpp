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

#include "staticlib/pion/http_message.hpp"

#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <algorithm>
#include <mutex>

#include "asio.hpp"

#include "staticlib/support.hpp"
#include "staticlib/utils.hpp"

#include "staticlib/pion/http_request.hpp"
#include "staticlib/pion/http_parser.hpp"
#include "staticlib/pion/tcp_connection.hpp"

namespace staticlib { 
namespace pion {

http_message::receive_error_t::~receive_error_t() STATICLIB_NOEXCEPT { }

const char* http_message::receive_error_t::name() const STATICLIB_NOEXCEPT {
    return "receive_error_t";
}

std::string http_message::receive_error_t::message(int ev) const {
    std::string result;
    switch (ev) {
    case 1:
        result = "HTTP message parsing error";
        break;
    default:
        result = "Unknown receive error";
        break;
    }
    return result;
}

// static members of message

// generic strings used by HTTP
const std::string http_message::STRING_EMPTY;
const std::string http_message::STRING_CRLF("\x0D\x0A");
const std::string http_message::STRING_HTTP_VERSION("HTTP/");
const std::string http_message::HEADER_NAME_VALUE_DELIMITER(": ");
const std::string http_message::COOKIE_NAME_VALUE_DELIMITER("=");

// common HTTP header names
const std::string http_message::HEADER_HOST("Host");
const std::string http_message::HEADER_COOKIE("Cookie");
const std::string http_message::HEADER_SET_COOKIE("Set-Cookie");
const std::string http_message::HEADER_CONNECTION("Connection");
const std::string http_message::HEADER_CONTENT_TYPE("Content-Type");
const std::string http_message::HEADER_CONTENT_LENGTH("Content-Length");
const std::string http_message::HEADER_CONTENT_LOCATION("Content-Location");
const std::string http_message::HEADER_CONTENT_ENCODING("Content-Encoding");
const std::string http_message::HEADER_CONTENT_DISPOSITION("Content-Disposition");
const std::string http_message::HEADER_LAST_MODIFIED("Last-Modified");
const std::string http_message::HEADER_IF_MODIFIED_SINCE("If-Modified-Since");
const std::string http_message::HEADER_TRANSFER_ENCODING("Transfer-Encoding");
const std::string http_message::HEADER_LOCATION("Location");
const std::string http_message::HEADER_AUTHORIZATION("Authorization");
const std::string http_message::HEADER_REFERER("Referer");
const std::string http_message::HEADER_USER_AGENT("User-Agent");
const std::string http_message::HEADER_X_FORWARDED_FOR("X-Forwarded-For");
const std::string http_message::HEADER_CLIENT_IP("Client-IP");

// common HTTP content types
const std::string http_message::CONTENT_TYPE_HTML("text/html");
const std::string http_message::CONTENT_TYPE_TEXT("text/plain");
const std::string http_message::CONTENT_TYPE_XML("text/xml");
const std::string http_message::CONTENT_TYPE_URLENCODED("application/x-www-form-urlencoded");
const std::string http_message::CONTENT_TYPE_MULTIPART_FORM_DATA("multipart/form-data");

// common HTTP request methods
const std::string http_message::REQUEST_METHOD_HEAD("HEAD");
const std::string http_message::REQUEST_METHOD_GET("GET");
const std::string http_message::REQUEST_METHOD_PUT("PUT");
const std::string http_message::REQUEST_METHOD_POST("POST");
const std::string http_message::REQUEST_METHOD_DELETE("DELETE");
const std::string http_message::REQUEST_METHOD_OPTIONS("OPTIONS");

// common HTTP response messages
const std::string http_message::RESPONSE_MESSAGE_OK("OK");
const std::string http_message::RESPONSE_MESSAGE_CREATED("Created");
const std::string http_message::RESPONSE_MESSAGE_ACCEPTED("Accepted");
const std::string http_message::RESPONSE_MESSAGE_NO_CONTENT("No Content");
const std::string http_message::RESPONSE_MESSAGE_FOUND("Found");
const std::string http_message::RESPONSE_MESSAGE_UNAUTHORIZED("Unauthorized");
const std::string http_message::RESPONSE_MESSAGE_FORBIDDEN("Forbidden");
const std::string http_message::RESPONSE_MESSAGE_NOT_FOUND("Not Found");
const std::string http_message::RESPONSE_MESSAGE_METHOD_NOT_ALLOWED("Method Not Allowed");
const std::string http_message::RESPONSE_MESSAGE_NOT_MODIFIED("Not Modified");
const std::string http_message::RESPONSE_MESSAGE_BAD_REQUEST("Bad Request");
const std::string http_message::RESPONSE_MESSAGE_SERVER_ERROR("Server Error");
const std::string http_message::RESPONSE_MESSAGE_NOT_IMPLEMENTED("Not Implemented");
const std::string http_message::RESPONSE_MESSAGE_CONTINUE("Continue");

// common HTTP response codes
const unsigned int http_message::RESPONSE_CODE_OK = 200;
const unsigned int http_message::RESPONSE_CODE_CREATED = 201;
const unsigned int http_message::RESPONSE_CODE_ACCEPTED = 202;
const unsigned int http_message::RESPONSE_CODE_NO_CONTENT = 204;
const unsigned int http_message::RESPONSE_CODE_FOUND = 302;
const unsigned int http_message::RESPONSE_CODE_UNAUTHORIZED = 401;
const unsigned int http_message::RESPONSE_CODE_FORBIDDEN = 403;
const unsigned int http_message::RESPONSE_CODE_NOT_FOUND = 404;
const unsigned int http_message::RESPONSE_CODE_METHOD_NOT_ALLOWED = 405;
const unsigned int http_message::RESPONSE_CODE_NOT_MODIFIED = 304;
const unsigned int http_message::RESPONSE_CODE_BAD_REQUEST = 400;
const unsigned int http_message::RESPONSE_CODE_SERVER_ERROR = 500;
const unsigned int http_message::RESPONSE_CODE_NOT_IMPLEMENTED = 501;
const unsigned int http_message::RESPONSE_CODE_CONTINUE = 100;

// response to "Expect: 100-Continue" header
// see https://groups.google.com/d/msg/mongoose-users/92fD1Elk5m4/Op6fPLZtlrEJ
const std::string http_message::RESPONSE_FULLMESSAGE_100_CONTINUE("HTTP/1.1 100 Continue\r\n\r\n");

const std::regex  http_message::REGEX_ICASE_CHUNKED(".*chunked.*", std::regex::icase);

http_message::http_message() : 
m_is_valid(false), 
m_is_chunked(false),
m_chunks_supported(false),
m_do_not_send_content_length(false),
m_version_major(1),
m_version_minor(1),
m_content_length(0),
m_content_buf(),
m_status(STATUS_NONE),
m_has_missing_packets(false),
m_has_data_after_missing(false) { }

http_message::http_message(const http_message& http_msg) : 
m_first_line(http_msg.m_first_line),
m_is_valid(http_msg.m_is_valid),
m_is_chunked(http_msg.m_is_chunked),
m_chunks_supported(http_msg.m_chunks_supported),
m_do_not_send_content_length(http_msg.m_do_not_send_content_length),
m_remote_ip(http_msg.m_remote_ip),
m_version_major(http_msg.m_version_major),
m_version_minor(http_msg.m_version_minor),
m_content_length(http_msg.m_content_length),
m_content_buf(http_msg.m_content_buf),
m_chunk_cache(http_msg.m_chunk_cache),
m_headers(http_msg.m_headers),
m_status(http_msg.m_status),
m_has_missing_packets(http_msg.m_has_missing_packets),
m_has_data_after_missing(http_msg.m_has_data_after_missing) { }

http_message::~http_message() { }

http_message& http_message::operator=(const http_message& http_msg) {
    m_first_line = http_msg.m_first_line;
    m_is_valid = http_msg.m_is_valid;
    m_is_chunked = http_msg.m_is_chunked;
    m_chunks_supported = http_msg.m_chunks_supported;
    m_do_not_send_content_length = http_msg.m_do_not_send_content_length;
    m_remote_ip = http_msg.m_remote_ip;
    m_version_major = http_msg.m_version_major;
    m_version_minor = http_msg.m_version_minor;
    m_content_length = http_msg.m_content_length;
    m_content_buf = http_msg.m_content_buf;
    m_chunk_cache = http_msg.m_chunk_cache;
    m_headers = http_msg.m_headers;
    m_status = http_msg.m_status;
    m_has_missing_packets = http_msg.m_has_missing_packets;
    m_has_data_after_missing = http_msg.m_has_data_after_missing;
    return *this;
}

void http_message::clear() {
    clear_first_line();
    m_is_valid = m_is_chunked = m_chunks_supported
            = m_do_not_send_content_length = false;
    m_remote_ip = asio::ip::address_v4(0);
    m_version_major = m_version_minor = 1;
    m_content_length = 0;
    m_content_buf.clear();
    m_chunk_cache.clear();
    m_headers.clear();
    m_cookie_params.clear();
    m_status = STATUS_NONE;
    m_has_missing_packets = false;
    m_has_data_after_missing = false;
}

bool http_message::is_valid() const {
    return m_is_valid;
}

bool http_message::get_chunks_supported() const {
    return m_chunks_supported;
}

const asio::ip::address& http_message::get_remote_ip() const {
    return m_remote_ip;
}

uint16_t http_message::get_version_major() const {
    return m_version_major;
}

uint16_t http_message::get_version_minor() const {
    return m_version_minor;
}

std::string http_message::get_version_string() const {
    std::string http_version(STRING_HTTP_VERSION);
    http_version += sl::support::to_string(get_version_major());
    http_version += '.';
    http_version += sl::support::to_string(get_version_minor());
    return http_version;
}

size_t http_message::get_content_length() const {
    return m_content_length;
}

bool http_message::is_chunked() const {
    return m_is_chunked;
}

bool http_message::is_content_buffer_allocated() const {
    return !m_content_buf.is_empty();
}

std::size_t http_message::get_content_buffer_size() const {
    return m_content_buf.size();
}

char* http_message::get_content() {
    return m_content_buf.get();
}

const char* http_message::get_content() const {
    return m_content_buf.get();
}

http_message::chunk_cache_type& http_message::get_chunk_cache() {
    return m_chunk_cache;
}

const std::string& http_message::get_header(const std::string& key) const {
    return get_value(m_headers, key);
}

std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& 
        http_message::get_headers() {
    return m_headers;
}

bool http_message::has_header(const std::string& key) const {
    return (m_headers.find(key) != m_headers.end());
}

const std::string& http_message::get_cookie(const std::string& key) const {
    return get_value(m_cookie_params, key);
}

std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& 
        http_message::get_cookies() {
    return m_cookie_params;
}

bool http_message::has_cookie(const std::string& key) const {
    return (m_cookie_params.find(key) != m_cookie_params.end());
}

void http_message::add_cookie(const std::string& key, const std::string& value) {
    m_cookie_params.insert(std::make_pair(key, value));
}

void http_message::change_cookie(const std::string& key, const std::string& value) {
    change_value(m_cookie_params, key, value);
}

void http_message::delete_cookie(const std::string& key) {
    delete_value(m_cookie_params, key);
}

const std::string& http_message::get_first_line() const {
    if (m_first_line.empty()) {
        update_first_line();
    }
    return m_first_line;
}

bool http_message::has_missing_packets() const {
    return m_has_missing_packets;
}

void http_message::set_missing_packets(bool newVal) {
    m_has_missing_packets = newVal;
}

bool http_message::has_data_after_missing_packets() const {
    return m_has_data_after_missing;
}

void http_message::set_data_after_missing_packet(bool newVal) {
    m_has_data_after_missing = newVal;
}

void http_message::set_is_valid(bool b) {
    m_is_valid = b;
}

void http_message::set_chunks_supported(bool b) {
    m_chunks_supported = b;
}

void http_message::set_remote_ip(const asio::ip::address& ip) {
    m_remote_ip = ip;
}

void http_message::set_version_major(const uint16_t n) {
    m_version_major = n;
    clear_first_line();
}

void http_message::set_version_minor(const uint16_t n) {
    m_version_minor = n;
    clear_first_line();
}

void http_message::set_content_length(size_t n) {
    m_content_length = n;
}

void http_message::set_do_not_send_content_length() {
    m_do_not_send_content_length = true;
}

http_message::data_status_type http_message::get_status() const {
    return m_status;
}

void http_message::set_status(data_status_type newVal) {
    m_status = newVal;
}

void http_message::update_content_length_using_header() {
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = m_headers.find(HEADER_CONTENT_LENGTH);
    if (i == m_headers.end()) {
        m_content_length = 0;
    } else {
        std::string trimmed_length(i->second);
        sl::utils::trim(trimmed_length);
        m_content_length = static_cast<size_t>(sl::utils::parse_uint64(trimmed_length));
    }
}

void http_message::update_transfer_encoding_using_header() {
    m_is_chunked = false;
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = m_headers.find(HEADER_TRANSFER_ENCODING);
    if (i != m_headers.end()) {
        // From RFC 2616, sec 3.6: All transfer-coding values are case-insensitive.
        m_is_chunked = std::regex_match(i->second, REGEX_ICASE_CHUNKED);
        // ignoring other possible values for now
    }
}

char* http_message::create_content_buffer() {
    m_content_buf.resize(m_content_length);
    return m_content_buf.get();
}

void http_message::set_content(const std::string& content) {
    set_content_length(content.size());
    create_content_buffer();
    memcpy(m_content_buf.get(), content.c_str(), content.size());
}

void http_message::clear_content() {
    set_content_length(0);
    create_content_buffer();
    delete_value(m_headers, HEADER_CONTENT_TYPE);
}

void http_message::set_content_type(const std::string& type) {
    change_value(m_headers, HEADER_CONTENT_TYPE, type);
}

void http_message::add_header(const std::string& key, const std::string& value) {
    m_headers.insert(std::make_pair(key, value));
}

void http_message::change_header(const std::string& key, const std::string& value) {
    change_value(m_headers, key, value);
}

void http_message::delete_header(const std::string& key) {
    delete_value(m_headers, key);
}

bool http_message::check_keep_alive() const {
    return (get_header(HEADER_CONNECTION) != "close"
            && (get_version_major() > 1
            || (get_version_major() >= 1 && get_version_minor() >= 1)));
}

std::string http_message::get_date_string(const time_t t) {
    // use mutex since time functions are normally not thread-safe
    static std::mutex time_mutex;
    static const char *TIME_FORMAT = "%a, %d %b %Y %H:%M:%S GMT";
    static const unsigned int TIME_BUF_SIZE = 100;
    char time_buf[TIME_BUF_SIZE + 1];

    std::unique_lock<std::mutex> time_lock(time_mutex);
    if (strftime(time_buf, TIME_BUF_SIZE, TIME_FORMAT, gmtime(&t)) == 0)
        time_buf[0] = '\0'; // failed; resulting buffer is indeterminate
    time_lock.unlock();

    return std::string(time_buf);
}

std::string http_message::make_query_string(const std::unordered_multimap<std::string, std::string,
        algorithm::ihash, algorithm::iequal_to>& query_params) {
    std::string query_string;
    for (std::unordered_multimap<std::string, std::string,
            algorithm::ihash, algorithm::iequal_to>::const_iterator i = query_params.begin();
            i != query_params.end(); ++i) {
        if (i != query_params.begin()) {
            query_string += '&';
        }
        query_string += sl::utils::url_encode(i->first);
        query_string += '=';
        query_string += sl::utils::url_encode(i->second);
    }
    return query_string;
}

std::string http_message::make_set_cookie_header(const std::string& name, const std::string& value,
        const std::string& path, const bool has_max_age, const unsigned long max_age) {
    // note: according to RFC6265, attributes should not be quoted
    std::string set_cookie_header(name);
    set_cookie_header += "=\"";
    set_cookie_header += value;
    set_cookie_header += "\"; Version=1";
    if (!path.empty()) {
        set_cookie_header += "; Path=";
        set_cookie_header += path;
    }
    if (has_max_age) {
        set_cookie_header += "; Max-Age=";
        set_cookie_header += sl::support::to_string(max_age);
    }
    return set_cookie_header;
}

void http_message::prepare_buffers_for_send(write_buffers_type& write_buffers, const bool keep_alive,
        const bool using_chunks) {
    // update message headers
    prepare_headers_for_send(keep_alive, using_chunks);
    // add first message line
    write_buffers.push_back(asio::buffer(get_first_line()));
    write_buffers.push_back(asio::buffer(STRING_CRLF));
    // append cookie headers (if any)
    append_cookie_headers();
    // append HTTP headers
    append_headers(write_buffers);
}


// message member functions

std::size_t http_message::send(tcp_connection& tcp_conn,
                          std::error_code& ec, bool headers_only) {
    // initialize write buffers for send operation using HTTP headers
    write_buffers_type write_buffers;
    prepare_buffers_for_send(write_buffers, tcp_conn.get_keep_alive(), false);

    // append payload content to write buffers (if there is any)
    if (!headers_only && get_content_length() > 0 && get_content() != NULL)
        write_buffers.push_back(asio::buffer(get_content(), get_content_length()));

    // send the message and return the result
    return tcp_conn.write(write_buffers, ec);
}

std::size_t http_message::receive(tcp_connection& tcp_conn,
                             std::error_code& ec,
                             http_parser& http_parser) {
    std::size_t last_bytes_read = 0;

    // make sure that we start out with an empty message
    clear();

    if (tcp_conn.get_pipelined()) {
        // there are pipelined messages available in the connection's read buffer
        const char *read_ptr;
        const char *read_end_ptr;
        tcp_conn.load_read_pos(read_ptr, read_end_ptr);
        last_bytes_read = (read_end_ptr - read_ptr);
        http_parser.set_read_buffer(read_ptr, last_bytes_read);
    } else {
        // read buffer is empty (not pipelined) -> read some bytes from the connection
        last_bytes_read = tcp_conn.read_some(ec);
        if (ec) return 0;
        // assert(last_bytes_read > 0);
        http_parser.set_read_buffer(tcp_conn.get_read_buffer().data(), last_bytes_read);
    }

    // incrementally read and parse bytes from the connection
    bool force_connection_closed = false;
    sl::support::tribool parse_result;
    for (;;) {
        // parse bytes available in the read buffer
        parse_result = http_parser.parse(*this, ec);
        if (! indeterminate(parse_result)) break;

        // read more bytes from the connection
        last_bytes_read = tcp_conn.read_some(ec);
        if (ec || last_bytes_read == 0) {
            if (http_parser.check_premature_eof(*this)) {
                // premature EOF encountered
                if (! ec)
                    ec = make_error_code(asio::error::misc_errors::eof);
                return http_parser.get_total_bytes_read();
            } else {
                // EOF reached when content length unknown
                // assume it is the correct end of content
                // and everything is OK
                force_connection_closed = true;
                parse_result = true;
                ec.clear();
                break;
            }
            break;
        }

        // update the HTTP parser's read buffer
        http_parser.set_read_buffer(tcp_conn.get_read_buffer().data(), last_bytes_read);
    }
    
    if (parse_result == false) {
        // an error occurred while parsing the message headers
        return http_parser.get_total_bytes_read();
    }

    // set the connection's lifecycle type
    if (!force_connection_closed && check_keep_alive()) {
        if ( http_parser.eof() ) {
            // the connection should be kept alive, but does not have pipelined messages
            tcp_conn.set_lifecycle(tcp_connection::LIFECYCLE_KEEPALIVE);
        } else {
            // the connection has pipelined messages
            tcp_conn.set_lifecycle(tcp_connection::LIFECYCLE_PIPELINED);
            
            // save the read position as a bookmark so that it can be retrieved
            // by a new HTTP parser, which will be created after the current
            // message has been handled
            const char *read_ptr;
            const char *read_end_ptr;
            http_parser.load_read_pos(read_ptr, read_end_ptr);
            tcp_conn.save_read_pos(read_ptr, read_end_ptr);
        }
    } else {
        // default to close the connection
        tcp_conn.set_lifecycle(tcp_connection::LIFECYCLE_CLOSE);
        
        // save the read position as a bookmark so that it can be retrieved
        // by a new HTTP parser
        if (http_parser.get_parse_headers_only()) {
            const char *read_ptr;
            const char *read_end_ptr;
            http_parser.load_read_pos(read_ptr, read_end_ptr);
            tcp_conn.save_read_pos(read_ptr, read_end_ptr);
        }
    }

    return (http_parser.get_total_bytes_read());
}

std::size_t http_message::receive(tcp_connection& tcp_conn,
                             std::error_code& ec,
                             bool headers_only,
                             std::size_t max_content_length) {
    http_parser http_parser(reinterpret_cast<http_request*>(this) != NULL);
    http_parser.parse_headers_only(headers_only);
    http_parser.set_max_content_length(max_content_length);
    return receive(tcp_conn, ec, http_parser);
}

std::size_t http_message::write(std::ostream& out,
    std::error_code& ec, bool headers_only) {
    // reset error_code
    ec.clear();

    // initialize write buffers for send operation using HTTP headers
    write_buffers_type write_buffers;
    prepare_buffers_for_send(write_buffers, true, false);

    // append payload content to write buffers (if there is any)
    if (!headers_only && get_content_length() > 0 && get_content() != NULL)
        write_buffers.push_back(asio::buffer(get_content(), get_content_length()));

    // write message to the output stream
    std::size_t bytes_out = 0;
    for (write_buffers_type::const_iterator i=write_buffers.begin(); i!=write_buffers.end(); ++i) {
        const char *ptr = asio::buffer_cast<const char*>(*i);
        size_t len = asio::buffer_size(*i);
        out.write(ptr, len);
        bytes_out += len;
    }

    return bytes_out;
}

std::size_t http_message::read(std::istream& in,
                          std::error_code& ec,
                          http_parser& http_parser) {
    // make sure that we start out with an empty message & clear error_code
    clear();
    ec.clear();
    
    // parse data from file one byte at a time
    sl::support::tribool parse_result;
    char c;
    while (in) {
        in.read(&c, 1);
        if ( ! in ) {
            ec = make_error_code(asio::error::misc_errors::eof);
            break;
        }
        http_parser.set_read_buffer(&c, 1);
        parse_result = http_parser.parse(*this, ec);
        if (! indeterminate(parse_result)) break;
    }

    if (indeterminate(parse_result)) {
        if (http_parser.check_premature_eof(*this)) {
            // premature EOF encountered
            if (! ec)
                ec = make_error_code(asio::error::misc_errors::eof);
        } else {
            // EOF reached when content length unknown
            // assume it is the correct end of content
            // and everything is OK
            parse_result = true;
            ec.clear();
        }
    }
    
    return (http_parser.get_total_bytes_read());
}

std::size_t http_message::read(std::istream& in,
                          std::error_code& ec,
                          bool headers_only,
                          std::size_t max_content_length) {
    http_parser http_parser(reinterpret_cast<http_request*>(this) != NULL);
    http_parser.parse_headers_only(headers_only);
    http_parser.set_max_content_length(max_content_length);
    return read(in, ec, http_parser);
}

void http_message::concatenate_chunks(void) {
    set_content_length(m_chunk_cache.size());
    char *post_buffer = create_content_buffer();
    if (m_chunk_cache.size() > 0)
        std::copy(m_chunk_cache.begin(), m_chunk_cache.end(), post_buffer);
}

http_message::content_buffer_t::~content_buffer_t() { }

http_message::content_buffer_t::content_buffer_t() : 
m_buf(), 
m_len(0),
m_empty(0),
m_ptr(&m_empty) { }

http_message::content_buffer_t::content_buffer_t(const content_buffer_t& buf) : 
m_buf(), 
m_len(0),
m_empty(0),
m_ptr(&m_empty) {
    if (buf.size()) {
        resize(buf.size());
        memcpy(get(), buf.get(), buf.size());
    }
}

http_message::content_buffer_t& http_message::content_buffer_t::operator=(const content_buffer_t& buf) {
    if (buf.size()) {
        resize(buf.size());
        memcpy(get(), buf.get(), buf.size());
    } else {
        clear();
    }
    return *this;
}

bool http_message::content_buffer_t::is_empty() const {
    return m_len == 0;
}

std::size_t http_message::content_buffer_t::size() const {
    return m_len;
}

const char* http_message::content_buffer_t::get() const {
    return m_ptr;
}

char* http_message::content_buffer_t::get() {
    return m_ptr;
}

void http_message::content_buffer_t::resize(std::size_t len) {
    m_len = len;
    if (len == 0) {
        m_buf.reset();
        m_ptr = &m_empty;
    } else {
        m_buf.reset(new char[len + 1]); 
        memset(m_buf.get(), '\0', len + 1);
        m_ptr = m_buf.get();
    }
}

void http_message::content_buffer_t::clear() {
    resize(0);
}

void http_message::prepare_headers_for_send(const bool keep_alive, const bool using_chunks) {
    change_header(HEADER_CONNECTION, (keep_alive ? "Keep-Alive" : "close"));
    if (using_chunks) {
        if (get_chunks_supported()) {
            change_header(HEADER_TRANSFER_ENCODING, "chunked");
        }
    } else if (!m_do_not_send_content_length) {
        change_header(HEADER_CONTENT_LENGTH, sl::support::to_string(get_content_length()));
    }
}

void http_message::append_headers(write_buffers_type& write_buffers) {
    // add HTTP headers
    for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = m_headers.begin(); i != m_headers.end(); ++i) {
        write_buffers.push_back(asio::buffer(i->first));
        write_buffers.push_back(asio::buffer(HEADER_NAME_VALUE_DELIMITER));
        write_buffers.push_back(asio::buffer(i->second));
        write_buffers.push_back(asio::buffer(STRING_CRLF));
    }
    // add an extra CRLF to end HTTP headers
    write_buffers.push_back(asio::buffer(STRING_CRLF));
}

void http_message::append_cookie_headers() { }

void http_message::clear_first_line() const {
    if (!m_first_line.empty()) {
        m_first_line.clear();
    }
}

} // namespace
}
