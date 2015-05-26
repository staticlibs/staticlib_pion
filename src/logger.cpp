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

#include "pion/logger.hpp"

namespace pion {

#if defined(PION_DISABLE_LOGGING)

logger::logger(int /* glog */) { }

logger::operator bool() const {
    return false;
}

#elif defined(PION_USE_OSTREAM_LOGGING)

logger::log_priority_type logger::m_priority = logger::LOG_LEVEL_INFO;

logger::~logger() { }

logger::logger() : m_name("pion") { }

logger::logger(const std::string& name) : m_name(name) { }

logger::logger(const logger& p) : m_name(p.m_name) { }

#endif
    
} // end namespace pion
