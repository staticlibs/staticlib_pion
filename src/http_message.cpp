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

} // namespace
}
