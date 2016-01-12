# Copyright 2015, alex at staticlibs.net
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required ( VERSION 2.8.12 )

# toolchain setup
set ( STATICLIB_TOOLCHAIN linux_amd64_gcc CACHE STRING "toolchain triplet" )
if ( NOT DEFINED STATICLIB_CMAKE )
    set ( STATICLIB_CMAKE ${CMAKE_CURRENT_LIST_DIR}/../../cmake CACHE INTERNAL "" )    
endif ( )
set ( CMAKE_TOOLCHAIN_FILE ${STATICLIB_CMAKE}/toolchains/${STATICLIB_TOOLCHAIN}.cmake CACHE INTERNAL "" )

# project
project ( staticlib_httpserver_test CXX )
include ( ${STATICLIB_CMAKE}/staticlibs_common.cmake )

# dependencies
# options, use SET ( OPTNAME ON CACHE BOOL "") in parent to override
set ( staticlib_httpserver_USE_LOG4CPLUS OFF CACHE BOOL "" )
set ( staticlib_httpserver_USE_OPENSSL OFF CACHE BOOL "" )
if ( NOT STATICLIB_TOOLCHAIN MATCHES "linux_[^_]+_[^_]+" )
    if ( staticlib_httpserver_USE_LOG4CPLUS )
        staticlib_add_subdirectory ( ${CMAKE_CURRENT_LIST_DIR}/deps/external_log4cplus )
    endif ( )
    if ( staticlib_httpserver_USE_OPENSSL AND ( NOT STATICLIB_TOOLCHAIN MATCHES "alpine_[^_]+_[^_]+" ) )
        staticlib_add_subdirectory ( ${CMAKE_CURRENT_LIST_DIR}/deps/external_openssl )
    endif ( )
endif ( )
staticlib_add_subdirectory ( ${CMAKE_CURRENT_LIST_DIR}/../../staticlib_httpserver )
set ( ${PROJECT_NAME}_DEPS staticlib_httpserver )
staticlib_pkg_check_modules ( ${PROJECT_NAME}_DEPS_PC REQUIRED ${PROJECT_NAME}_DEPS )

# tests
set ( ${PROJECT_NAME}_TEST_INCLUDES ${${PROJECT_NAME}_DEPS_PC_INCLUDE_DIRS} )
set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_DEPS_PC_LIBRARIES} )
if ( STATICLIB_TOOLCHAIN MATCHES "alpine_[^_]+_[^_]+" )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} z )
endif ( )
set ( ${PROJECT_NAME}_TEST_OPTS ${${PROJECT_NAME}_DEPS_PC_CFLAGS_OTHER} )
if ( ${CMAKE_CXX_COMPILER_ID} MATCHES "(Clang|GNU)" )
    set ( ${PROJECT_NAME}_TEST_OPTS ${${PROJECT_NAME}_TEST_OPTS} -Wno-deprecated-declarations )
endif ( )
staticlib_enable_testing ( ${PROJECT_NAME}_TEST_INCLUDES ${PROJECT_NAME}_TEST_LIBS ${PROJECT_NAME}_TEST_OPTS )
