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

#ifndef STATICLIB_PION_LOGGER_HPP
#define STATICLIB_PION_LOGGER_HPP

#include "staticlib/config.hpp"

#if defined(STATICLIB_PION_DISABLE_LOGGING)

#define STATICLIB_PION_LOG_DEBUG(LOG, MSG)    { if (LOG) {} }
#define STATICLIB_PION_LOG_INFO(LOG, MSG)     { if (LOG) {} }
#define STATICLIB_PION_LOG_WARN(LOG, MSG)     { if (LOG) {} }
#define STATICLIB_PION_LOG_ERROR(LOG, MSG)    { if (LOG) {} }
#define STATICLIB_PION_LOG_FATAL(LOG, MSG)    { if (LOG) {} }

#elif defined(STATICLIB_PION_USE_WILTON_LOGGING)

#include <sstream>
#include <string>

#include "wilton/wilton_logging.h"

namespace staticlib {
namespace pion {
namespace logger {

/**
 * Send log message to wilton_logging
 * 
 * @param name logger name
 * @param level logging level
 * @param message message
 * 
 */
inline void log(const std::string& name, const std::string& level, const std::string& message){
    wilton_logger_log(level.c_str(), static_cast<int>(level.length()),
            name.c_str(), static_cast<int>(name.length()),
            message.c_str(), static_cast<int>(message.length()));
}

/**
 * Check whether specified logging level is enabled
 * 
 * @param name logger name
 * @param level level to check
 * @returns true if enabled, false otherwise
 */
inline bool is_priority_enabled(const std::string& name, const std::string& level){
    int res = 0;
    wilton_logger_is_level_enabled(name.c_str(), static_cast<int>(name.length()),
            level.c_str(), static_cast<int>(level.length()), std::addressof(res));
    return res != 0;
}

} // namespace
}
}

// Logging api, used in pion, send MSG as [str1 << str2], ostringstream used to handle it.
#define STATICLIB_PION_LOG_TO_WILTON(LOG, LEVEL, MSG) \
    if (logger::is_priority_enabled(LOG, LEVEL)) { \
        std::ostringstream ostream; \
        ostream << MSG; \
        logger::log(LOG, LEVEL, ostream.str()); \
    }

#define STATICLIB_PION_LOG_DEBUG(LOG, MSG) STATICLIB_PION_LOG_TO_WILTON(LOG, "DEBUG", MSG)
#define STATICLIB_PION_LOG_INFO(LOG, MSG) STATICLIB_PION_LOG_TO_WILTON(LOG, "INFO", MSG)
#define STATICLIB_PION_LOG_WARN(LOG, MSG) STATICLIB_PION_LOG_TO_WILTON(LOG, "WARN", MSG)
#define STATICLIB_PION_LOG_ERROR(LOG, MSG) STATICLIB_PION_LOG_TO_WILTON(LOG, "ERROR", MSG)
#define STATICLIB_PION_LOG_FATAL(LOG, MSG) STATICLIB_PION_LOG_TO_WILTON(LOG, "FATAL", MSG)

#else

// Logging uses std::cout and std::cerr
#include <iostream>
#include <string>
#include <ctime>

#define STATICLIB_PION_LOG_TO_OSTREAM(LOG, LEVEL, MSG) \
{ std::cout << time(nullptr) << ' ' << LEVEL << ' ' << LOG << ' ' << MSG << std::endl; }

#define STATICLIB_PION_LOG_DEBUG(LOG, MSG) STATICLIB_PION_LOG_TO_OSTREAM(LOG, "DEBUG", MSG)
#define STATICLIB_PION_LOG_INFO(LOG, MSG) STATICLIB_PION_LOG_TO_OSTREAM(LOG, "INFO", MSG)
#define STATICLIB_PION_LOG_WARN(LOG, MSG) STATICLIB_PION_LOG_TO_OSTREAM(LOG, "WARN", MSG)
#define STATICLIB_PION_LOG_ERROR(LOG, MSG) STATICLIB_PION_LOG_TO_OSTREAM(LOG, "ERROR", MSG)
#define STATICLIB_PION_LOG_FATAL(LOG, MSG) STATICLIB_PION_LOG_TO_OSTREAM(LOG, "FATAL", MSG)

#endif

#endif // STATICLIB_PION_LOGGER_HPP
