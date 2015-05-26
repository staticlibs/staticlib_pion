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

#ifndef __PION_PIONCONFIG_HEADER__
#define __PION_PIONCONFIG_HEADER__

// http://stackoverflow.com/a/18387764/314015
#ifndef _MSC_VER
#define PION_NOEXCEPT noexcept
#else
#define PION_NOEXCEPT
#endif // _MSC_VER

/* Define to the version number of pion. */
#ifndef PION_VERSION
#define PION_VERSION "5.0.7"
#endif // PION_VERSION

#if defined(_WIN32) || defined(_WINDOWS)
    #define PION_WIN32  1
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT    0x0501
    #endif
#endif // _WIN32

#ifdef _MSC_VER
    #ifdef PION_EXPORTS
        #define PION_API __declspec(dllexport)
    #elif defined PION_STATIC_LINKING
        #define PION_API
    #else
        #define PION_API __declspec(dllimport)
    #endif

    #ifdef PION_STATIC_LINKING
        #define PION_PLUGIN 
    #else
        #define PION_PLUGIN __declspec(dllexport)
    #endif

    /*
    Verify correctness of the PION_STATIC_LINKING setup
    */
    #ifdef PION_STATIC_LINKING
        #ifdef _USRDLL
            #error Need to be compiled as a static library for PION_STATIC_LINKING
        #endif
    #endif

#else

    /* This is used by Windows projects to flag exported symbols */
    #define PION_API
    #define PION_PLUGIN

#endif // _MSC_VER

#endif //__PION_PIONCONFIG_HEADER__
