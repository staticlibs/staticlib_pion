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

#ifndef STATICLIB_PION_HTTP_MESSAGE_HPP
#define STATICLIB_PION_HTTP_MESSAGE_HPP

#include <cstring>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "asio.hpp"

#include "staticlib/config.hpp"

#include "staticlib/pion/algorithm.hpp"

namespace staticlib {
namespace pion {

// forward declaration for class used by send() and receive()
class tcp_connection;
// forward declaration of parser class
class http_parser;

/**
 * Base container for HTTP messages
 */
class http_message  {
public:

    /**
     * Data type for I/O write buffers (these wrap existing data to be sent)
     */
    using write_buffers_type = std::vector<asio::const_buffer>;

    /**
     * Used to cache chunked data
     */
    using chunk_cache_type = std::vector<char>; 

    /**
     * Defines message data integrity status codes
     */
    enum data_status_type {
        /**
         * No data received (i.e. all lost)
         */
        STATUS_NONE,

        /**
         * One or more missing packets at the end
         */
        STATUS_TRUNCATED,

        /**
         * One or more missing packets but NOT at the end
         */
        STATUS_PARTIAL,

        /**
         * No missing packets
         */
        STATUS_OK
    };

    /**
     * Data type for library errors returned during receive() operations
     */
    struct receive_error_t : public asio::error_category {
        /**
         * Destructor
         */
        virtual ~receive_error_t() STATICLIB_NOEXCEPT;

        /**
         * Returns error category name
         * 
         * @return error category name
         */
        virtual const char* name() const STATICLIB_NOEXCEPT;

        /**
         * Returns error message for specified code
         * 
         * @param ev error code
         * @return error message
         */
        virtual std::string message(int ev) const;
    };

    // generic strings used by HTTP
    static const std::string STRING_EMPTY;
    static const std::string STRING_CRLF;
    static const std::string STRING_HTTP_VERSION;
    static const std::string HEADER_NAME_VALUE_DELIMITER;
    static const std::string COOKIE_NAME_VALUE_DELIMITER;

    // common HTTP header names
    static const std::string HEADER_HOST;
    static const std::string HEADER_COOKIE;
    static const std::string HEADER_SET_COOKIE;
    static const std::string HEADER_CONNECTION;
    static const std::string HEADER_CONTENT_TYPE;
    static const std::string HEADER_CONTENT_LENGTH;
    static const std::string HEADER_CONTENT_LOCATION;
    static const std::string HEADER_CONTENT_ENCODING;
    static const std::string HEADER_CONTENT_DISPOSITION;
    static const std::string HEADER_LAST_MODIFIED;
    static const std::string HEADER_IF_MODIFIED_SINCE;
    static const std::string HEADER_TRANSFER_ENCODING;
    static const std::string HEADER_LOCATION;
    static const std::string HEADER_AUTHORIZATION;
    static const std::string HEADER_REFERER;
    static const std::string HEADER_USER_AGENT;
    static const std::string HEADER_X_FORWARDED_FOR;
    static const std::string HEADER_CLIENT_IP;

    // common HTTP content types
    static const std::string CONTENT_TYPE_HTML;
    static const std::string CONTENT_TYPE_TEXT;
    static const std::string CONTENT_TYPE_XML;
    static const std::string CONTENT_TYPE_URLENCODED;
    static const std::string CONTENT_TYPE_MULTIPART_FORM_DATA;

    // common HTTP request methods
    static const std::string REQUEST_METHOD_HEAD;
    static const std::string REQUEST_METHOD_GET;
    static const std::string REQUEST_METHOD_PUT;
    static const std::string REQUEST_METHOD_POST;
    static const std::string REQUEST_METHOD_DELETE;
    static const std::string REQUEST_METHOD_OPTIONS;

    // common HTTP response messages
    static const std::string RESPONSE_MESSAGE_OK;
    static const std::string RESPONSE_MESSAGE_CREATED;
    static const std::string RESPONSE_MESSAGE_ACCEPTED;
    static const std::string RESPONSE_MESSAGE_NO_CONTENT;
    static const std::string RESPONSE_MESSAGE_FOUND;
    static const std::string RESPONSE_MESSAGE_UNAUTHORIZED;
    static const std::string RESPONSE_MESSAGE_FORBIDDEN;
    static const std::string RESPONSE_MESSAGE_NOT_FOUND;
    static const std::string RESPONSE_MESSAGE_METHOD_NOT_ALLOWED;
    static const std::string RESPONSE_MESSAGE_NOT_MODIFIED;
    static const std::string RESPONSE_MESSAGE_BAD_REQUEST;
    static const std::string RESPONSE_MESSAGE_SERVER_ERROR;
    static const std::string RESPONSE_MESSAGE_NOT_IMPLEMENTED;
    static const std::string RESPONSE_MESSAGE_CONTINUE;

    // common HTTP response codes
    static const unsigned int RESPONSE_CODE_OK;
    static const unsigned int RESPONSE_CODE_CREATED;
    static const unsigned int RESPONSE_CODE_ACCEPTED;
    static const unsigned int RESPONSE_CODE_NO_CONTENT;
    static const unsigned int RESPONSE_CODE_FOUND;
    static const unsigned int RESPONSE_CODE_UNAUTHORIZED;
    static const unsigned int RESPONSE_CODE_FORBIDDEN;
    static const unsigned int RESPONSE_CODE_NOT_FOUND;
    static const unsigned int RESPONSE_CODE_METHOD_NOT_ALLOWED;
    static const unsigned int RESPONSE_CODE_NOT_MODIFIED;
    static const unsigned int RESPONSE_CODE_BAD_REQUEST;
    static const unsigned int RESPONSE_CODE_SERVER_ERROR;
    static const unsigned int RESPONSE_CODE_NOT_IMPLEMENTED;
    static const unsigned int RESPONSE_CODE_CONTINUE;

    // response to "Expect: 100-Continue" header
    static const std::string RESPONSE_FULLMESSAGE_100_CONTINUE;

protected:
 
    /**
     * First line sent in an HTTP message
     * (i.e. "GET / HTTP/1.1" for request, or "HTTP/1.1 200 OK" for response)
     */
    mutable std::string m_first_line;

    /**
     * A simple helper class used to manage a fixed-size payload content buffer
     */
    class content_buffer_t {
        std::unique_ptr<char[]> m_buf;
        std::size_t m_len;
        char m_empty;
        char *m_ptr;
    public:
        /**
         * Simple destructor
         */
        ~content_buffer_t();

        /**
         * Default constructor
         */
        content_buffer_t();

        /**
         * Copy constructor
         */
        content_buffer_t(const content_buffer_t& buf);

        /**
         * Assignment operator
         */
        content_buffer_t& operator=(const content_buffer_t& buf);

        /**
         * Returns true if buffer is empty
         */
        bool is_empty() const;

        /**
         * Returns size in bytes
         */
        std::size_t size() const;

        /**
         * Returns const pointer to data
         */
        const char *get() const;

        /**
         * Returns mutable pointer to data
         */
        char *get();

        /**
         * Changes the size of the content buffer
         */
        void resize(std::size_t len);

        /**
         * Clears the content buffer
         */
        void clear();
    };
    
private:
    /**
     * True if the HTTP message is valid
     */
    bool m_is_valid;

    /**
     * Whether the message body is chunked
     */
    bool m_is_chunked;

    /**
     * True if chunked transfer encodings are supported
     */
    bool m_chunks_supported;

    /**
     * If true, the content length will not be sent in the HTTP headers
     */
    bool m_do_not_send_content_length;

    /**
     * IP address of the remote endpoint
     */
    asio::ip::address m_remote_ip;

    /**
     * HTTP major version number
     */
    uint16_t m_version_major;

    /**
     * HTTP major version number
     */
    uint16_t m_version_minor;

    /**
     * The length of the payload content (in bytes)
     */
    size_t m_content_length;

    /**
     * The payload content, if any was sent with the message
     */
    content_buffer_t m_content_buf;

    /**
     * Buffers for holding chunked data
     */
    chunk_cache_type m_chunk_cache;

    /**
     * HTTP message headers
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to> m_headers;

    /**
     * HTTP cookie parameters parsed from the headers
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to> m_cookie_params;

    /**
     * Message data integrity status
     */
    data_status_type m_status;

    /**
     * Missing packet indicator
     */
    bool m_has_missing_packets;

    /**
     * Indicates missing packets in the middle of the data stream
     */
    bool m_has_data_after_missing;    

public:

    /**
     * Constructs a new HTTP message object
     */
    http_message();

    /**
     * Copy constructor
     */
    http_message(const http_message& http_msg);

    /**
     * Assignment operator
     * 
     * @param http_msg source instance
     * @return copied instance
     */
    http_message& operator=(const http_message& http_msg);

    /**
     * Virtual destructor
     */
    virtual ~http_message();

    /**
     * Clears all message data
     */
    virtual void clear();

    /**
     * Should return true if the content length can be implied without headers
     * 
     * @return true if the content length can be implied without headers
     */
    virtual bool is_content_length_implied(void) const = 0;

    /**
     * Returns true if the message is valid
     */
    bool is_valid() const;

    /**
     * Returns true if chunked transfer encodings are supported
     */
    bool get_chunks_supported() const;

    /**
     * Returns IP address of the remote endpoint
     */
    const asio::ip::address& get_remote_ip() const;

    /**
     * Returns the major HTTP version number
     * 
     * @return major HTTP version number
     */
    uint16_t get_version_major() const;

    /**
     * Returns the minor HTTP version number
     */
    uint16_t get_version_minor() const;

    /**
     * Returns a string representation of the HTTP version (i.e. "HTTP/1.1")
     */
    std::string get_version_string() const;

    /**
     * Returns the length of the payload content (in bytes)
     * 
     * @return length of the payload content (in bytes)
     */
    size_t get_content_length() const;

    /**
     * Returns true if the message content is chunked
     * 
     * @return true if the message content is chunked
     */
    bool is_chunked(void) const;

    /**
     * Returns true if buffer for content is allocated
     */
    bool is_content_buffer_allocated() const;
    
    /**
     * Returns size of allocated buffer
     */
    std::size_t get_content_buffer_size() const;

    /**
     * Returns a pointer to the payload content, or empty string if there is none
     */
    char *get_content();

    /**
     * Returns a const pointer to the payload content, or empty string if there is none
     * 
     * @return const pointer to the payload content, or empty string if there is none
     */
    const char *get_content() const;

    /**
     * Returns a reference to the chunk cache
     */
    chunk_cache_type& get_chunk_cache();

    /**
     * Returns a value for the header if any are defined; otherwise, an empty string
     */
    const std::string& get_header(const std::string& key) const;

    /**
     * Returns a reference to the HTTP headers multimap
     * 
     * @return reference to the HTTP headers multimap
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& get_headers();

    /**
     * Returns true if at least one value for the header is defined
     * 
     * @param key header name
     * @return true if at least one value for the header is defined
     */
    bool has_header(const std::string& key) const;

    /**
     * Returns a value for the cookie if any are defined; otherwise, an empty string
     * since cookie names are insensitive, key should use lowercase alpha chars
     * 
     * @param key cookie name
     * @return value for the cookie if any are defined; otherwise, an empty string
     */
    const std::string& get_cookie(const std::string& key) const;
    
    /**
     * Returns the cookie parameters
     * 
     * @return cookie parameters multimap
     */
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& get_cookies();

    /**
     * Returns true if at least one value for the cookie is defined
     * since cookie names are insensitive, key should use lowercase alpha chars
     * 
     * @param key cookie name
     * @return true if at least one value for the cookie is defined
     */
    bool has_cookie(const std::string& key) const;

    /**
     * Adds a value for the cookie since cookie names are insensitive,
     * key should use lowercase alpha chars
     * 
     * @param key cookie name
     * @param value cookie value
     */
    void add_cookie(const std::string& key, const std::string& value);

    /**
     * Changes the value of a cookie
     * since cookie names are insensitive, key should use lowercase alpha chars
     * 
     * @param key cookie name
     * @param value cookie value
     */
    void change_cookie(const std::string& key, const std::string& value);

    /**
     * Removes all values for a cookie
     * since cookie names are insensitive, key should use lowercase alpha chars
     * 
     * @param key cookie name
     */
    void delete_cookie(const std::string& key);
    
    /**
     * Returns a string containing the first line for the HTTP message
     * 
     * @return string containing the first line for the HTTP message
     */
    const std::string& get_first_line() const;

    /**
     * Returns true if there were missing packets 
     * 
     * @return true if there were missing packets 
     */
    bool has_missing_packets() const;

    /**
     * Set to true when missing packets detected
     * 
     * @param newVal missing packets flag
     */
    void set_missing_packets(bool newVal);

    /**
     * Returns true if more data seen after the missing packets
     * 
     * @return true if more data seen after the missing packets
     */
    bool has_data_after_missing_packets() const;

    /**
     * Sets data seen after the missing packets flag
     * 
     * @param newVal data seen after the missing packets flag
     */
    void set_data_after_missing_packet(bool newVal);

    /**
     * Sets whether or not the message is valid
     * 
     * @param b is-valid flag
     */
    void set_is_valid(bool b = true);

    /**
     * Set to true if chunked transfer encodings are supported
     * 
     * @param b chunked transfer encodings are supported flag
     */
    void set_chunks_supported(bool b);

    /**
     * Sets IP address of the remote endpoint
     * 
     * @param ip IP address of the remote endpoint
     */
    void set_remote_ip(const asio::ip::address& ip);

    /**
     * Sets the major HTTP version number
     * 
     * @param n major HTTP version number
     */
    void set_version_major(const uint16_t n);

    /**
     * Sets the minor HTTP version number
     * 
     * @param n minor HTTP version number
     */
    void set_version_minor(const uint16_t n);

    /**
     * Sets the length of the payload content (in bytes)
     * 
     * @param n length of the payload content (in bytes)
     */
    void set_content_length(size_t n);

    /**
     * If called, the content-length will not be sent in the HTTP headers
     */
    void set_do_not_send_content_length();

    /**
     * Return the data receival status
     * 
     * @return data receival status
     */
    data_status_type get_status() const;

    /**
     * Sets data receival status
     * 
     * @param newVal data receival status
     */
    void set_status(data_status_type newVal);

    /**
     * Sets the length of the payload content using the Content-Length header
     */
    void update_content_length_using_header();

    /**
     * Sets the transfer coding using the Transfer-Encoding header
     */
    void update_transfer_encoding_using_header();

    /**
     * Creates a payload content buffer of size m_content_length and returns
     * a pointer to the new buffer (memory is managed by message class)
     * 
     * @return pointer to newly created content buffer
     */
    char *create_content_buffer();

    /**
     * Resets payload content to match the value of a string
     * 
     * @param content payload content
     */
    void set_content(const std::string& content);

    /**
     * Clears payload content buffer
     */
    void clear_content();

    /**
     * Sets the content type for the message payload
     * 
     * @param type content type
     */
    void set_content_type(const std::string& type);

    /**
     * Adds a value for the HTTP header named key
     * 
     * @param key header name
     * @param value header value
     */
    void add_header(const std::string& key, const std::string& value);

    /**
     * Changes the value for the HTTP header named key
     * 
     * @param key header name
     * @param value header value
     */
    void change_header(const std::string& key, const std::string& value);

    /**
     * Removes all values for the HTTP header named key
     * 
     * @param key header name
     */
    void delete_header(const std::string& key);

    /**
     * Returns true if the HTTP connection may be kept alive
     * 
     * @return true if the HTTP connection may be kept alive
     */
    bool check_keep_alive() const;

    /**
     * Initializes a vector of write buffers with the HTTP message information
     *
     * @param write_buffers vector of write buffers to initialize
     * @param keep_alive true if the connection should be kept alive
     * @param using_chunks true if the payload content will be sent in chunks
     */
    void prepare_buffers_for_send(write_buffers_type& write_buffers, const bool keep_alive,
            const bool using_chunks);

    /**
     * Pieces together all the received chunks
     */
    void concatenate_chunks();

protected:

    /**
     * Builds an HTTP query string from a collection of query parameters
     * 
     * @param query_params query parameters
     * @return query string
     */
    std::string make_query_string(const std::unordered_multimap<std::string, std::string,
            algorithm::ihash, algorithm::iequal_to>& query_params);

    /**
     * Creates a "Set-Cookie" header
     *
     * @param name the name of the cookie
     * @param value the value of the cookie
     * @param path the path of the cookie
     * @param has_max_age true if the max_age value should be set
     * @param max_age the life of the cookie, in seconds (0 = discard)
     *
     * @return the new "Set-Cookie" header
     */
    std::string make_set_cookie_header(const std::string& name, const std::string& value,
            const std::string& path, const bool has_max_age = false, const unsigned long max_age = 0); 

    /**
     * Prepares HTTP headers for a send operation
     *
     * @param keep_alive true if the connection should be kept alive
     * @param using_chunks true if the payload content will be sent in chunks
     */
    void prepare_headers_for_send(const bool keep_alive, const bool using_chunks);

    /**
     * Appends the message's HTTP headers to a vector of write buffers
     *
     * @param write_buffers the buffers to append HTTP headers into
     */
    void append_headers(write_buffers_type& write_buffers);

    /**
     * Appends HTTP headers for any cookies defined by the http::message
     */
    void append_cookie_headers();

    /**
     * Returns the first value in a dictionary if key is found; or an empty
     * string if no values are found
     *
     * @param dict the dictionary to search for key
     * @param key the key to search for
     * @return value if found; empty string if not
     */
    template <typename DictionaryType>
    static const std::string& get_value(const DictionaryType& dict, const std::string& key) {
        typename DictionaryType::const_iterator i = dict.find(key);
        return ( (i==dict.end()) ? STRING_EMPTY : i->second );
    }

    /**
     * Changes the value for a dictionary key.  Adds the key if it does not
     * already exist.  If multiple values exist for the key, they will be
     * removed and only the new value will remain.
     *
     * @param dict the dictionary object to update
     * @param key the key to change the value for
     * @param value the value to assign to the key
     */
    template <typename DictionaryType>
    inline static void change_value(DictionaryType& dict, const std::string& key, 
            const std::string& value) {
        // retrieve all current values for key
        std::pair<typename DictionaryType::iterator, typename DictionaryType::iterator>
            result_pair = dict.equal_range(key);
        if (result_pair.first == dict.end()) {
            // no values exist -> add a new key
            dict.insert(std::make_pair(key, value));
        } else {
            // set the first value found for the key to the new one
            result_pair.first->second = value;
            // remove any remaining values
            typename DictionaryType::iterator i;
            ++(result_pair.first);
            while (result_pair.first != result_pair.second) {
                i = result_pair.first;
                ++(result_pair.first);
                dict.erase(i);
            }
        }
    }

    /**
     * Deletes all values for a key
     *
     * @param dict the dictionary object to update
     * @param key the key to delete
     */
    template <typename DictionaryType>
    inline static void delete_value(DictionaryType& dict, const std::string& key) {
        std::pair<typename DictionaryType::iterator, typename DictionaryType::iterator>
            result_pair = dict.equal_range(key);
        if (result_pair.first != dict.end()) {
            dict.erase(result_pair.first, result_pair.second);
        }
    }

    /**
     * Erases the string containing the first line for the HTTP message
     * (it will be updated the next time get_first_line() is called)
     */
    void clear_first_line() const;

    /**
     * Updates the string containing the first line for the HTTP message
     */
    virtual void update_first_line() const = 0;

};


} // namespace
}

#endif // STATICLIB_PION_HTTP_MESSAGE_HPP
