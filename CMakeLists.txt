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
set ( CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/toolchains/${STATICLIB_TOOLCHAIN}.cmake CACHE INTERNAL "" )

# project setup
project ( staticlib_httpserver CXX )
set ( ${PROJECT_NAME}_STATICLIB_VERSION 5.0.7.4 )

# common include
include ( ${CMAKE_CURRENT_LIST_DIR}/cmake/staticlibs_common.cmake )
# options, use SET ( OPTNAME ON CACHE BOOL "") in parent to override
option ( STATICLIB_PION_DISABLE_LOGGING "Disable logging to std out and err" OFF )
option ( STATICLIB_PION_USE_STATICLIB_LOG4CPLUS "Use log4cplus lib from Staticlibs" OFF )
option ( STATICLIB_PION_USE_SYSTEM_LOG4CPLUS "Use system log4cplus lib" OFF )
option ( STATICLIB_PION_USE_STATICLIB_OPENSSL "Use OpenSSL lib from Staticlibs" OFF )
option ( STATICLIB_PION_USE_SYSTEM_OPENSSL "Use system OpenSSL lib" OFF )

# build steps
set ( ${PROJECT_NAME}_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/deps/asio/asio/include )
set ( ${PROJECT_NAME}_DEFINITIONS
        -DASIO_STANDALONE
        -DASIO_HAS_STD_CHRONO )
set ( ${PROJECT_NAME}_OPTIONS "" )
if ( STATICLIB_TOOLCHAIN MATCHES "windows_[^_]+_msvc" )    
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS}
            -D_CRT_SECURE_NO_WARNINGS
            -D_SCL_SECURE_NO_WARNINGS
            -D_WIN32_WINNT=0x0501 
            -DPION_CMAKE_BUILD
            -DPION_STATIC_LINKING )
endif ( )
if ( STATICLIB_PION_USE_STATICLIB_LOG4CPLUS )
    staticlib_add_subdirectory ( deps external_log4cplus )
    set ( ${PROJECT_NAME}_INCLUDES ${${PROJECT_NAME}_INCLUDES} ${external_log4cplus_INCLUDES} )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_USE_LOG4CPLUS )
elseif ( STATICLIB_PION_USE_SYSTEM_LOG4CPLUS )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_USE_LOG4CPLUS )
elseif ( STATICLIB_PION_DISABLE_LOGGING )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_DISABLE_LOGGING )
endif ( )
if ( STATICLIB_PION_USE_STATICLIB_OPENSSL )
    staticlib_add_subdirectory ( deps external_openssl )
    set ( ${PROJECT_NAME}_INCLUDES ${${PROJECT_NAME}_INCLUDES} ${external_openssl_INCLUDES} )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_HAVE_SSL )
elseif ( STATICLIB_PION_USE_SYSTEM_OPENSSL )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_HAVE_SSL )
endif ( )
if ( STATICLIB_TOOLCHAIN MATCHES "[^_]+_[^_]+_clang" )
    EXECUTE_PROCESS( COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE clang_full_version_string )
    string (REGEX REPLACE ".*clang version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${clang_full_version_string})
    if ( CLANG_VERSION_STRING VERSION_GREATER 3.5 )
        set ( ${PROJECT_NAME}_OPTIONS ${${PROJECT_NAME}_OPTIONS} -Wno-unused-local-typedef )
    endif ( )
endif ( )
if ( STATICLIB_TOOLCHAIN MATCHES "[^_]+_[^_]+_gcc" )
    # asio warnings with openssl on
    set ( ${PROJECT_NAME}_OPTIONS ${${PROJECT_NAME}_OPTIONS} -Wno-unused-variable )
endif ( )
file ( GLOB_RECURSE ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp )
add_library ( ${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRC} )
target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE ${${PROJECT_NAME}_INCLUDES} )
target_compile_definitions ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_DEFINITIONS} )
target_compile_options ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_OPTIONS} )
# force building Staticlibs openssl first
if ( STATICLIB_PION_USE_STATICLIB_OPENSSL )
    add_dependencies ( ${PROJECT_NAME} external_openssl )
endif ( )

# tests
set ( ${PROJECT_NAME}_TEST_INCLUDES ${${PROJECT_NAME}_INCLUDES} )
set ( ${PROJECT_NAME}_TEST_LIBS ${PROJECT_NAME} )
set ( ${PROJECT_NAME}_TEST_OPTS ${${PROJECT_NAME}_DEFINITIONS} ${${PROJECT_NAME}_OPTIONS} )
if ( STATICLIB_TOOLCHAIN MATCHES "linux_[^_]+_[^_]+" )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} pthread )
endif ( )
if ( STATICLIB_PION_USE_STATICLIB_LOG4CPLUS )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} external_log4cplus )
    set ( ${PROJECT_NAME}_TEST_OPTS ${${PROJECT_NAME}_TEST_OPTS} -DPION_USE_LOG4CPLUS_STATIC )
elseif ( STATICLIB_PION_USE_SYSTEM_LOG4CPLUS )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} log4cplus )
endif ( )
if ( STATICLIB_PION_USE_STATICLIB_OPENSSL )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} external_openssl external_openssl_ssl )
elseif ( STATICLIB_PION_USE_SYSTEM_OPENSSL )
    set ( ${PROJECT_NAME}_TEST_LIBS ${${PROJECT_NAME}_TEST_LIBS} ssl crypto )
endif ( )
staticlib_enable_testing ( ${PROJECT_NAME}_TEST_INCLUDES ${PROJECT_NAME}_TEST_LIBS ${PROJECT_NAME}_TEST_OPTS )

# docs
staticlib_enable_docs ( )

# export headers
set ( ${PROJECT_NAME}_INCLUDES ${${PROJECT_NAME}_INCLUDES} CACHE INTERNAL "" )
