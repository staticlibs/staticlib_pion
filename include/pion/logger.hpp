// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_LOGGER_HEADER__
#define __PION_LOGGER_HEADER__

#include <pion/config.hpp>


#if defined(PION_USE_LOG4CPLUS)

    // log4cplus headers
    #include <log4cplus/logger.h>
    #include <log4cplus/loggingmacros.h>

    namespace pion {
        typedef log4cplus::Logger   logger;
        typedef log4cplus::Appender log_appender;
        typedef log4cplus::SharedAppenderPtr    log_appender_ptr;
    }

    #define PION_LOG_CONFIG_BASIC   log4cplus::BasicConfigurator::doConfigure();
    #define PION_LOG_CONFIG(FILE)   log4cplus::PropertyConfigurator::doConfigure(FILE);
    #define PION_GET_LOGGER(NAME)   log4cplus::Logger::getInstance(NAME)
    #define PION_SHUTDOWN_LOGGER    log4cplus::Logger::shutdown();

    #define PION_LOG_SETLEVEL_DEBUG(LOG)    LOG.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
    #define PION_LOG_SETLEVEL_INFO(LOG)     LOG.setLogLevel(log4cplus::INFO_LOG_LEVEL);
    #define PION_LOG_SETLEVEL_WARN(LOG)     LOG.setLogLevel(log4cplus::WARN_LOG_LEVEL);
    #define PION_LOG_SETLEVEL_ERROR(LOG)    LOG.setLogLevel(log4cplus::ERROR_LOG_LEVEL);
    #define PION_LOG_SETLEVEL_FATAL(LOG)    LOG.setLogLevel(log4cplus::FATAL_LOG_LEVEL);
    #define PION_LOG_SETLEVEL_UP(LOG)       LOG.setLogLevel(LOG.getLogLevel()+1);
    #define PION_LOG_SETLEVEL_DOWN(LOG)     LOG.setLogLevel(LOG.getLogLevel()-1);

    #define PION_LOG_DEBUG  LOG4CPLUS_DEBUG
    #define PION_LOG_INFO   LOG4CPLUS_INFO
    #define PION_LOG_WARN   LOG4CPLUS_WARN
    #define PION_LOG_ERROR  LOG4CPLUS_ERROR
    #define PION_LOG_FATAL  LOG4CPLUS_FATAL


#elif defined(PION_DISABLE_LOGGING)

    // Logging is disabled -> add do-nothing stubs for logging
    namespace pion {
        struct PION_API logger {
            logger(int /* glog */) {}
            operator bool() const { return false; }
            static void shutdown() {}
        };
        typedef int     log_appender;
        typedef log_appender *   log_appender_ptr;
    }

    #define PION_LOG_CONFIG_BASIC   {}
    #define PION_LOG_CONFIG(FILE)   {}
    #define PION_GET_LOGGER(NAME)   0
    #define PION_SHUTDOWN_LOGGER    0

    // use LOG to avoid warnings about LOG not being used
    #define PION_LOG_SETLEVEL_DEBUG(LOG)    { if (LOG) {} }
    #define PION_LOG_SETLEVEL_INFO(LOG)     { if (LOG) {} }
    #define PION_LOG_SETLEVEL_WARN(LOG)     { if (LOG) {} }
    #define PION_LOG_SETLEVEL_ERROR(LOG)    { if (LOG) {} }
    #define PION_LOG_SETLEVEL_FATAL(LOG)    { if (LOG) {} }
    #define PION_LOG_SETLEVEL_UP(LOG)       { if (LOG) {} }
    #define PION_LOG_SETLEVEL_DOWN(LOG)     { if (LOG) {} }

    #define PION_LOG_DEBUG(LOG, MSG)    { if (LOG) {} }
    #define PION_LOG_INFO(LOG, MSG)     { if (LOG) {} }
    #define PION_LOG_WARN(LOG, MSG)     { if (LOG) {} }
    #define PION_LOG_ERROR(LOG, MSG)    { if (LOG) {} }
    #define PION_LOG_FATAL(LOG, MSG)    { if (LOG) {} }
#else

    #define PION_USE_OSTREAM_LOGGING

    // Logging uses std::cout and std::cerr
    #include <iostream>
    #include <string>
    #include <ctime>

    namespace pion {
        struct PION_API logger {
            enum log_priority_type {
                LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN,
                LOG_LEVEL_ERROR, LOG_LEVEL_FATAL
            };
            ~logger() {}
            logger(void) : m_name("pion") {}
            logger(const std::string& name) : m_name(name) {}
            logger(const logger& p) : m_name(p.m_name) {}
            static void shutdown() {}
            std::string                 m_name;
            static log_priority_type     m_priority;
        };
        typedef int     log_appender;
        typedef log_appender *   log_appender_ptr;
    }

    #define PION_LOG_CONFIG_BASIC   {}
    #define PION_LOG_CONFIG(FILE)   {}
    #define PION_GET_LOGGER(NAME)   pion::logger(NAME)
    #define PION_SHUTDOWN_LOGGER    {}

    #define PION_LOG_SETLEVEL_DEBUG(LOG)    { LOG.m_priority = pion::logger::LOG_LEVEL_DEBUG; }
    #define PION_LOG_SETLEVEL_INFO(LOG)     { LOG.m_priority = pion::logger::LOG_LEVEL_INFO; }
    #define PION_LOG_SETLEVEL_WARN(LOG)     { LOG.m_priority = pion::logger::LOG_LEVEL_WARN; }
    #define PION_LOG_SETLEVEL_ERROR(LOG)    { LOG.m_priority = pion::logger::LOG_LEVEL_ERROR; }
    #define PION_LOG_SETLEVEL_FATAL(LOG)    { LOG.m_priority = pion::logger::LOG_LEVEL_FATAL; }
    #define PION_LOG_SETLEVEL_UP(LOG)       { ++LOG.m_priority; }
    #define PION_LOG_SETLEVEL_DOWN(LOG)     { --LOG.m_priority; }

    #define PION_LOG_DEBUG(LOG, MSG)    if (LOG.m_priority <= pion::logger::LOG_LEVEL_DEBUG) { std::cout << time(NULL) << " DEBUG " << LOG.m_name << ' ' << MSG << std::endl; }
    #define PION_LOG_INFO(LOG, MSG)     if (LOG.m_priority <= pion::logger::LOG_LEVEL_INFO) { std::cout << time(NULL) << " INFO " << LOG.m_name << ' ' << MSG << std::endl; }
    #define PION_LOG_WARN(LOG, MSG)     if (LOG.m_priority <= pion::logger::LOG_LEVEL_WARN) { std::cerr << time(NULL) << " WARN " << LOG.m_name << ' ' << MSG << std::endl; }
    #define PION_LOG_ERROR(LOG, MSG)    if (LOG.m_priority <= pion::logger::LOG_LEVEL_ERROR) { std::cerr << time(NULL) << " ERROR " << LOG.m_name << ' ' << MSG << std::endl; }
    #define PION_LOG_FATAL(LOG, MSG)    if (LOG.m_priority <= pion::logger::LOG_LEVEL_FATAL) { std::cerr << time(NULL) << " FATAL " << LOG.m_name << ' ' << MSG << std::endl; }

#endif

#endif
