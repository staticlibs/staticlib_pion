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

#ifndef STATICLIB_PION_HTTP_PARSER_HPP
#define STATICLIB_PION_HTTP_PARSER_HPP

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

#include "staticlib/config.hpp"
#include "staticlib/support.hpp"

#include "staticlib/pion/algorithm.hpp"
#include "staticlib/pion/logger.hpp"
#include "staticlib/pion/http_message.hpp"

namespace staticlib { 
namespace pion {

// forward declarations used for finishing HTTP messages
class http_request;
class http_response;

/**
 * Parses HTTP messages
 */
class http_parser {
public:

    /**
     * Maximum length for HTTP payload content (for non-streaming server)
     */
    static const size_t DEFAULT_CONTENT_MAX;

    /**
     * Callback type used to consume payload content
     */
    using payload_handler_type = std::function<void(const char *, size_t)>;

    /**
     * Class-specific error code values
     */
    enum error_value_t {
        ERROR_METHOD_CHAR = 1,
        ERROR_METHOD_SIZE,
        ERROR_URI_CHAR,
        ERROR_URI_SIZE,
        ERROR_QUERY_CHAR,
        ERROR_QUERY_SIZE,
        ERROR_VERSION_EMPTY,
        ERROR_VERSION_CHAR,
        ERROR_STATUS_EMPTY,
        ERROR_STATUS_CHAR,
        ERROR_HEADER_CHAR,
        ERROR_HEADER_NAME_SIZE,
        ERROR_HEADER_VALUE_SIZE,
        ERROR_INVALID_CONTENT_LENGTH,
        ERROR_CHUNK_CHAR,
        ERROR_MISSING_CHUNK_DATA,
        ERROR_MISSING_HEADER_DATA,
        ERROR_MISSING_TOO_MUCH_CONTENT,
    };

    /**
     * Class-specific error category
     */
    class error_category_t : public asio::error_category {
    public:
        /**
         * Returns error category name
         * 
         * @return error category name
         */
        const char *name() const STATICLIB_NOEXCEPT {
            return "parser";
        }
        
        /**
         * Returns error message for specified code
         * 
         * @param ev error code
         * @return error message
         */
        std::string message(int ev) const;
    };

protected:

    /**
     * Maximum length for response status message
     */
    static const uint32_t STATUS_MESSAGE_MAX;

    /**
     * Maximum length for the request method
     */
    static const uint32_t METHOD_MAX;

    /**
     * Maximum length for the resource requested
     */
    static const uint32_t RESOURCE_MAX;

    /**
     * Maximum length for the query string
     */
    static const uint32_t QUERY_STRING_MAX;

    /**
     * Maximum length for an HTTP header name
     */
    static const uint32_t HEADER_NAME_MAX;

    /**
     * Maximum length for an HTTP header value
     */
    static const uint32_t HEADER_VALUE_MAX;

    /**
     * Maximum length for the name of a query string variable
     */
    static const uint32_t QUERY_NAME_MAX;

    /**
     * Maximum length for the value of a query string variable
     */
    static const uint32_t QUERY_VALUE_MAX;

    /**
     * Maximum length for the name of a cookie name
     */
    static const uint32_t COOKIE_NAME_MAX;

    /**
     * Maximum length for the value of a cookie; also used for path and domain
     */
    static const uint32_t COOKIE_VALUE_MAX;

    /**
     * Points to the next character to be consumed in the read_buffer
     */
    const char * m_read_ptr;

    /**
     * Points to the end of the read_buffer (last byte + 1)
     */
    const char * m_read_end_ptr;

private:
    /**
     * State used to keep track of where we are in parsing the HTTP message
     */
    enum message_parse_state_type {
        PARSE_START,
        PARSE_HEADERS,
        PARSE_FOOTERS,
        PARSE_CONTENT,
        PARSE_CONTENT_NO_LENGTH,
        PARSE_CHUNKS, PARSE_END
    };

    /**
     * State used to keep track of where we are in parsing the HTTP headers
     * (only used if message_parse_state_t == PARSE_HEADERS)
     */
    enum header_parse_state_type {
        PARSE_METHOD_START,
        PARSE_METHOD,
        PARSE_URI_STEM,
        PARSE_URI_QUERY,
        PARSE_HTTP_VERSION_H,
        PARSE_HTTP_VERSION_T_1,
        PARSE_HTTP_VERSION_T_2,
        PARSE_HTTP_VERSION_P,
        PARSE_HTTP_VERSION_SLASH,
        PARSE_HTTP_VERSION_MAJOR_START,
        PARSE_HTTP_VERSION_MAJOR,
        PARSE_HTTP_VERSION_MINOR_START,
        PARSE_HTTP_VERSION_MINOR,
        PARSE_STATUS_CODE_START,
        PARSE_STATUS_CODE,
        PARSE_STATUS_MESSAGE,
        PARSE_EXPECTING_NEWLINE,
        PARSE_EXPECTING_CR,
        PARSE_HEADER_WHITESPACE,
        PARSE_HEADER_START,
        PARSE_HEADER_NAME,
        PARSE_SPACE_BEFORE_HEADER_VALUE,
        PARSE_HEADER_VALUE,
        PARSE_EXPECTING_FINAL_NEWLINE,
        PARSE_EXPECTING_FINAL_CR
    };

    /**
     * State used to keep track of where we are in parsing chunked content
     * (only used if message_parse_state_t == PARSE_CHUNKS)
     */
    enum chunk_parse_state_type {
        PARSE_CHUNK_SIZE_START,
        PARSE_CHUNK_SIZE,
        PARSE_EXPECTING_IGNORED_TEXT_AFTER_CHUNK_SIZE,
        PARSE_EXPECTING_CR_AFTER_CHUNK_SIZE,
        PARSE_EXPECTING_LF_AFTER_CHUNK_SIZE,
        PARSE_CHUNK,
        PARSE_EXPECTING_CR_AFTER_CHUNK,
        PARSE_EXPECTING_LF_AFTER_CHUNK,
        PARSE_EXPECTING_FINAL_CR_OR_FOOTERS_AFTER_LAST_CHUNK,
        PARSE_EXPECTING_FINAL_LF_AFTER_LAST_CHUNK
    };

    /**
     * The current state of parsing HTTP headers
     */
    message_parse_state_type m_message_parse_state;

    /**
     * The current state of parsing HTTP headers
     */
    header_parse_state_type m_headers_parse_state;

    /**
     * The current state of parsing chunked content
     */
    chunk_parse_state_type m_chunked_content_parse_state;

    /**
     * If defined, this function is used to consume payload content
     */
    payload_handler_type* m_payload_handler = nullptr;

    /**
     * Used for parsing the HTTP response status code
     */
    uint16_t m_status_code;

    /**
     * Used for parsing the HTTP response status message
     */
    std::string m_status_message;

    /**
     * Used for parsing the request method
     */
    std::string m_method;

    /**
     * Used for parsing the name of resource requested
     */
    std::string m_resource;

    /**
     * Used for parsing the query string portion of a URI
     */
    std::string m_query_string;

    /**
     * Used to store the raw contents of HTTP headers when m_save_raw_headers is true
     */
    std::string m_raw_headers;

    /**
     * Used for parsing the name of HTTP headers
     */
    std::string m_header_name;

    /**
     * Used for parsing the value of HTTP headers
     */
    std::string m_header_value;

    /**
     * Used for parsing the chunk size
     */
    std::string m_chunk_size_str;

    /**
     * Number of bytes in the chunk currently being parsed
     */
    size_t m_size_of_current_chunk;

    /**
     * Number of bytes read so far in the chunk currently being parsed
     */
    size_t m_bytes_read_in_current_chunk;

    /**
     * Number of payload content bytes that have not yet been read
     */
    size_t m_bytes_content_remaining;

    /**
     * Number of bytes read so far into the message's payload content
     */
    size_t m_bytes_content_read;

    /**
     * Number of bytes read during last parse operation
     */
    size_t m_bytes_last_read;

    /**
     * Total number of bytes read while parsing the HTTP message
     */
    size_t m_bytes_total_read;

    /**
     * Maximum length for HTTP payload content
     */
    size_t m_max_content_length;

    /**
     * If true, then only HTTP headers will be parsed (no content parsing)
     */
    bool m_parse_headers_only;

    /**
     * If true, the raw contents of HTTP headers are stored into m_raw_headers
     */
    bool m_save_raw_headers;

    /**
     * Points to a single and unique instance of the parser error_category_t
     */
    static error_category_t * m_error_category_ptr;

    /**
     * Used to ensure thread safety of the parser error_category_t
     */
    static std::once_flag m_instance_flag;

public:

    /**
     * Creates new parser objects
     *
     * @param max_content_length maximum length for HTTP payload content
     */
    http_parser(size_t max_content_length = DEFAULT_CONTENT_MAX) :
    m_read_ptr(nullptr),
    m_read_end_ptr(nullptr),
    m_message_parse_state(PARSE_START),
    m_headers_parse_state(PARSE_METHOD_START),
    m_chunked_content_parse_state(PARSE_CHUNK_SIZE_START),
    m_status_code(0),
    m_bytes_content_remaining(0),
    m_bytes_content_read(0),
    m_bytes_last_read(0),
    m_bytes_total_read(0),
    m_max_content_length(max_content_length),
    m_parse_headers_only(false),
    m_save_raw_headers(false) { }

    /**
     * Deleted copy constructor
     */
    http_parser(const http_parser&) = delete;

    /**
     * Deleted copy assignment operator
     */
    http_parser& operator=(const http_parser&) = delete;

    /**
     * Default destructor
     */
    virtual ~http_parser() STATICLIB_NOEXCEPT { }

    /**
     * Parses an HTTP message including all payload content it might contain
     *
     * @param http_msg the HTTP message object to populate from parsing
     * @param ec error_code contains additional information for parsing errors
     *
     * @return tribool result of parsing:
     *                        false = message has an error,
     *                        true = finished parsing HTTP message,
     *                        indeterminate = not yet finished parsing HTTP message
     */
    sl::support::tribool parse(http_message& http_msg, std::error_code& ec);

    /**
     * Finishes parsing an HTTP response message
     *
     * @param http_msg the HTTP message object to finish
     */
    void finish(http_message& http_msg) const;

    /**
     * Resets the location and size of the read buffer
     *
     * @param ptr pointer to the first bytes available to be read
     * @param len number of bytes available to be read
     */
    void set_read_buffer(const char *ptr, size_t len) {
        m_read_ptr = ptr;
        m_read_end_ptr = ptr + len;
    }

    /**
     * Loads a read position bookmark
     *
     * @param read_ptr points to the next character to be consumed in the read_buffer
     * @param read_end_ptr points to the end of the read_buffer (last byte + 1)
     */
    void load_read_pos(const char *&read_ptr, const char *&read_end_ptr) const {
        read_ptr = m_read_ptr;
        read_end_ptr = m_read_end_ptr;
    }

    /**
     * Checks to see if a premature EOF was encountered while parsing.  This
     * should be called if there is no more data to parse, and if the last
     * call to the parse() function returned indeterminate
     *
     * @param http_msg the HTTP message object being parsed
     * @return true if premature EOF, false if message is OK & finished parsing
     */
    bool check_premature_eof(http_message& http_msg) {
        if (m_message_parse_state != PARSE_CONTENT_NO_LENGTH) {
            return true;
        }
        m_message_parse_state = PARSE_END;
        http_msg.concatenate_chunks();
        finish(http_msg);
        return false;
    }

    /**
     * Controls headers-only parsing (default is disabled; content parsed also)
     *
     * @param b if true, then the parse() function returns true after headers
     */
    void parse_headers_only(bool b = true) {
        m_parse_headers_only = b;
    }

    /**
     * Skip parsing all headers and parse payload content only
     *
     * @param http_msg the HTTP message object being parsed
     */
    void skip_header_parsing(http_message& http_msg) {
        std::error_code ec;
        finish_header_parsing(http_msg, ec);
    }
    
    /**
     * Resets the parser to its initial state
     */
    void reset() {
        m_message_parse_state = PARSE_START;
        m_headers_parse_state = PARSE_METHOD_START;
        m_chunked_content_parse_state = PARSE_CHUNK_SIZE_START;
        m_status_code = 0;
        m_status_message.erase();
        m_method.erase();
        m_resource.erase();
        m_query_string.erase();
        m_raw_headers.erase();
        m_bytes_content_read = m_bytes_last_read = m_bytes_total_read = 0;
    }

    /**
     * Returns true if there are no more bytes available in the read buffer
     * 
     * @return true if there are no more bytes available in the read buffer
     */
    bool eof() const {
        return m_read_ptr == nullptr || m_read_ptr >= m_read_end_ptr;
    }

    /**
     * Returns the number of bytes available in the read buffer
     * 
     * @return number of bytes available in the read buffer
     */
    size_t bytes_available() const {
        return (eof() ? 0 : (size_t)(m_read_end_ptr - m_read_ptr));
    }

    /**
     * Returns the number of bytes read during the last parse operation
     * 
     * @return number of bytes read during the last parse operation
     */
    size_t gcount() const {
        return m_bytes_last_read;
    }

    /**
     * Returns the total number of bytes read while parsing the HTTP message
     * 
     * @return total number of bytes read while parsing the HTTP message
     */
    size_t get_total_bytes_read() const {
        return m_bytes_total_read;
    }

    /**
     * Returns the total number of bytes read while parsing the payload content
     * 
     * @return total number of bytes read while parsing the payload content
     */
    size_t get_content_bytes_read() const {
        return m_bytes_content_read;
    }

    /**
     * Returns the maximum length for HTTP payload content
     * 
     * @return maximum length for HTTP payload content
     */
    size_t get_max_content_length() const {
        return m_max_content_length;
    }

    /**
     * Returns the raw HTTP headers saved by the parser
     * 
     * @return raw HTTP headers saved by the parser
     */
    const std::string& get_raw_headers() const {
        return m_raw_headers;
    }

    /**
     * Returns true if the parser is saving raw HTTP header contents
     */
    bool get_save_raw_headers() const {
        return m_save_raw_headers;
    }

    /**
     * Returns true if parsing headers only
     * 
     * @return true if parsing headers only
     */
    bool get_parse_headers_only() {
        return m_parse_headers_only;
    }
    
    /**
     * Returns true if the parser is being used to parse an HTTP request
     * 
     * @return true if the parser is being used to parse an HTTP request
     */
    bool is_parsing_request() const;

    /**
     * Returns true if the parser is being used to parse an HTTP response
     * 
     * @return true if the parser is being used to parse an HTTP response
     */
    bool is_parsing_response() const;

    /**
     * Defines a callback function to be used for consuming payload content
     * 
     * @param h a callback function to be used for consuming payload content
     */
    void set_payload_handler(payload_handler_type& h) {
        m_payload_handler = &h;
    }

    /**
     * Sets the maximum length for HTTP payload content
     * 
     * @param n maximum length for HTTP payload content
     */
    void set_max_content_length(size_t n) {
        m_max_content_length = n;
    }

    /**
     * Resets the maximum length for HTTP payload content to the default value
     */
    void reset_max_content_length() {
        m_max_content_length = DEFAULT_CONTENT_MAX;
    }

    /**
     * Sets parameter for saving raw HTTP header content
     * 
     * @param b parameter for saving raw HTTP header content
     */
    void set_save_raw_headers(bool b) {
        m_save_raw_headers = b;
    }

    /**
     * Parses a URI string
     *
     * @param uri the string to parse
     * @param proto will be set to the protocol (i.e. "http")
     * @param host will be set to the hostname (i.e. "www.cloudmeter.com")
     * @param port host port number to use for connection (i.e. 80)
     * @param path uri stem or file path
     * @param query uri query string
     *
     * @return true if the URI was successfully parsed, false if there was an error
     */
    static bool parse_uri(const std::string& uri, std::string& proto, std::string& host,
            uint16_t& port, std::string& path, std::string& query);

    /**
     * Parse key-value pairs out of a url-encoded string
     * (i.e. this=that&a=value)
     * 
     * @param dict dictionary for key-values pairs
     * @param ptr points to the start of the encoded string
     * @param len length of the encoded string, in bytes
     * 
     * @return bool true if successful
     */
    static bool parse_url_encoded(std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& dict,
            const char *ptr, const size_t len);

    /**
     * Parse key-value pairs out of a multipart/form-data payload content
     * (http://www.ietf.org/rfc/rfc2388.txt)
     *
     * @param dict dictionary for key-values pairs
     * @param content_type value of the content-type HTTP header
     * @param ptr points to the start of the encoded data
     * @param len length of the encoded data, in bytes
     *
     * @return bool true if successful
     */
    static bool parse_multipart_form_data(std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& dict,
            const std::string& content_type, const char *ptr, const size_t len);

    /**
     * Parse key-value pairs out of a "Cookie" request header
     * (i.e. this=that; a=value)
     * 
     * @param dict dictionary for key-values pairs
     * @param ptr points to the start of the header string to be parsed
     * @param len length of the encoded string, in bytes
     * @param set_cookie_header set true if parsing Set-Cookie response header
     * 
     * @return bool true if successful
     */
    static bool parse_cookie_header(std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& dict,
            const char *ptr, const size_t len, bool set_cookie_header);

    /**
     * Parse key-value pairs out of a "Cookie" request header
     * (i.e. this=that; a=value)
     * 
     * @param dict dictionary for key-values pairs
     * @param cookie_header header string to be parsed
     * @param set_cookie_header set true if parsing Set-Cookie response header
     * 
     * @return bool true if successful
     */
    static bool parse_cookie_header(std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& dict,
            const std::string& cookie_header, bool set_cookie_header);

    /**
     * Parse key-value pairs out of a url-encoded string
     * (i.e. this=that&a=value)
     * 
     * @param dict dictionary for key-values pairs
     * @param query the encoded query string to be parsed
     * 
     * @return bool true if successful
     */
    static bool parse_url_encoded(std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& dict,
            const std::string& query);

    /**
     * Parse key-value pairs out of a multipart/form-data payload content
     * (http://www.ietf.org/rfc/rfc2388.txt)
     *
     * @param dict dictionary for key-values pairs
     * @param content_type value of the content-type HTTP header
     * @param form_data the encoded form data
     *
     * @return bool true if successful
     */
    static bool parse_multipart_form_data(std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& dict,
            const std::string& content_type, const std::string& form_data);
 
    /**
     * Should be called after parsing HTTP headers, to prepare for payload content parsing
     * available in the read buffer
     *
     * @param http_msg the HTTP message object to populate from parsing
     * @param ec error_code contains additional information for parsing errors
     *
     * @return tribool result of parsing:
     *                        false = message has an error,
     *                        true = finished parsing HTTP message (no content),
     *                        indeterminate = payload content is available to be parsed
     */
    sl::support::tribool finish_header_parsing(http_message& http_msg, std::error_code& ec);

    /**
     * Returns an instance of parser::error_category_t
     * 
     * @return instance of parser::error_category_t
     */
    static error_category_t& get_error_category() {
        std::call_once(m_instance_flag, http_parser::create_error_category);
        return *m_error_category_ptr;
    }

protected:

    /**
     * Called after we have finished parsing the HTTP message headers
     * 
     * @param 
     * @param 
     */
    virtual void finished_parsing_headers(const std::error_code& /* ec */, sl::support::tribool& /* rc */) {
        // no-op
    }
    
    /**
     * Parses an HTTP message up to the end of the headers using bytes 
     * available in the read buffer
     *
     * @param http_msg the HTTP message object to populate from parsing
     * @param ec error_code contains additional information for parsing errors
     *
     * @return tribool result of parsing:
     *                        false = message has an error,
     *                        true = finished parsing HTTP headers,
     *                        indeterminate = not yet finished parsing HTTP headers
     */
    sl::support::tribool parse_headers(http_message& http_msg, std::error_code& ec);

    /**
     * Updates an http::message object with data obtained from parsing headers
     *
     * @param http_msg the HTTP message object to populate from parsing
     */
    void update_message_with_header_data(http_message& http_msg) const;

    /**
     * Parses a chunked HTTP message-body using bytes available in the read buffer
     *
     * @param chunk_buffers buffers to be populated from parsing chunked content
     * @param ec error_code contains additional information for parsing errors
     *
     * @return tribool result of parsing:
     *                        false = message has an error,
     *                        true = finished parsing message,
     *                        indeterminate = message is not yet finished
     */
    sl::support::tribool parse_chunks(std::vector<char>& chunk_buffers, std::error_code& ec);

    /**
     * Consumes payload content in the parser's read buffer 
     *
     * @param http_msg the HTTP message object to consume content for
     * @param ec error_code contains additional information for parsing errors
     *  
     * @return tribool result of parsing:
     *                        false = message has an error,
     *                        true = finished parsing message,
     *                        indeterminate = message is not yet finished
     */
    sl::support::tribool consume_content(http_message& http_msg, std::error_code& ec);

    /**
     * Consume the bytes available in the read buffer, converting them into
     * the next chunk for the HTTP message
     *
     * @param chunk_buffers buffers to be populated from parsing chunked content
     * @return size_t number of content bytes consumed, if any
     */
    size_t consume_content_as_next_chunk(std::vector<char>& chunk_buffers);

    /**
     * Compute and sets a HTTP Message data integrity status
     * @param http_msg target HTTP message 
     * @param msg_parsed_ok message parsing result
     */
    static void compute_msg_status(http_message& http_msg, bool msg_parsed_ok);

    /**
     * Sets an error code
     *
     * @param ec error code variable to define
     * @param ev error value to raise
     */
    static void set_error(std::error_code& ec, error_value_t ev) {
        ec = std::error_code(static_cast<int> (ev), get_error_category());
    }

    /**
     * Creates the unique parser error_category_t
     */
    static void create_error_category() {
        static error_category_t UNIQUE_ERROR_CATEGORY;
        m_error_category_ptr = &UNIQUE_ERROR_CATEGORY;
    }

    // misc functions used by the parsing functions

    static bool is_digit(int c) {
        return (c >= '0' && c <= '9');
    }
    
    static bool is_hex_digit(int c) {
        return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
    }
    
    static bool is_cookie_attribute(const std::string& name, bool set_cookie_header);

};

} // namespace
}

#endif // STATICLIB_PION_HTTP_PARSER_HPP
