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

# project
project ( staticlib_httpserver CXX )
set ( ${PROJECT_NAME}_STATICLIB_VERSION 5.0.7-13 )
set ( ${PROJECT_NAME}_DESCRIPTION "Embedded async HTTP 1.1 server based on a source code from https://github.com/splunk/pion" )
set ( ${PROJECT_NAME}_URL https://github.com/staticlibs/staticlib_httpserver )
include ( ${CMAKE_CURRENT_LIST_DIR}/resources/macros.cmake )

# docs
option ( ${PROJECT_NAME}_ENABLE_DOCS "Generate doxyfile and exit build" OFF )
if ( ${PROJECT_NAME}_ENABLE_DOCS )
    configure_file ( ${CMAKE_CURRENT_LIST_DIR}/resources/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile )
    return ( )
endif ( )

# check deplibs cache
if ( STATICLIB_USE_DEPLIBS_CACHE )
    set ( ${PROJECT_NAME}_CACHED_LIB_PATH ${STATICLIB_DEPLIBS_CACHE_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_STATIC_LIBRARY_SUFFIX} )
    if ( EXISTS ${${PROJECT_NAME}_CACHED_LIB_PATH} )
        add_library( ${PROJECT_NAME} STATIC IMPORTED GLOBAL )
        set_target_properties( ${PROJECT_NAME} PROPERTIES IMPORTED_LOCATION ${${PROJECT_NAME}_CACHED_LIB_PATH} )
        message ( STATUS "Using cached library: [${${PROJECT_NAME}_CACHED_LIB_PATH}]" )
        return ( )
    endif ( )
endif ( )

# options, use set ( OPTNAME ON CACHE BOOL "") in parent to override
option ( ${PROJECT_NAME}_DISABLE_LOGGING "Disable logging to std out and err" OFF )
option ( ${PROJECT_NAME}_USE_LOG4CPLUS "Use log4cplus lib for logging" OFF )
option ( ${PROJECT_NAME}_USE_OPENSSL "Use OpenSSL lib for https" OFF )

# standalone build
if ( NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY )
    set ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} )
endif ( )
if ( NOT DEFINED STATICLIB_TOOLCHAIN )
    if ( NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
        if ( ${PROJECT_NAME}_USE_LOG4CPLUS )
            staticlib_httpserver_add_subdirectory ( ${CMAKE_CURRENT_LIST_DIR}/../external_log4cplus )
        endif ( )
        if ( ${PROJECT_NAME}_USE_OPENSSL )
            staticlib_httpserver_add_subdirectory ( ${CMAKE_CURRENT_LIST_DIR}/../external_openssl )
        endif ( )
    endif (  )
endif ( )

# dependencies
set ( ${PROJECT_NAME}_DEPS "" )
if ( ${PROJECT_NAME}_USE_LOG4CPLUS )
    set ( ${PROJECT_NAME}_DEPS ${${PROJECT_NAME}_DEPS} log4cplus )
endif ( )
if ( ${PROJECT_NAME}_USE_OPENSSL )
    set ( ${PROJECT_NAME}_DEPS ${${PROJECT_NAME}_DEPS} openssl )
endif ( )
if ( ${PROJECT_NAME}_USE_LOG4CPLUS OR ${PROJECT_NAME}_USE_OPENSSL )
    staticlib_httpserver_pkg_check_modules ( ${PROJECT_NAME}_DEPS_PC REQUIRED ${PROJECT_NAME}_DEPS )
endif ( )

# library
file ( GLOB_RECURSE ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp )
file ( GLOB_RECURSE ${PROJECT_NAME}_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include/*.hpp )
add_library ( ${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRC} ${${PROJECT_NAME}_HEADERS} )
source_group ( "src" FILES ${${PROJECT_NAME}_SRC} )
source_group ( "include" FILES ${${PROJECT_NAME}_HEADERS} )
set ( ${PROJECT_NAME}_DEFINITIONS
        -DASIO_STANDALONE
        -DASIO_HAS_STD_CHRONO )
set ( ${PROJECT_NAME}_OPTIONS "" )
set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_DEFINITIONS} )
if ( ${CMAKE_CXX_COMPILER_ID}x MATCHES "MSVCx" )    
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS}
            -D_CRT_SECURE_NO_WARNINGS
            -D_SCL_SECURE_NO_WARNINGS
            -D_WIN32_WINNT=0x0501 
            -DWIN32_LEAN_AND_MEAN
            -DSTATICLIB_HTTPSERVER_CMAKE_BUILD)
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} 
            -D_WIN32_WINNT=0x0501
            -DWIN32_LEAN_AND_MEAN )
endif ( )
if ( ${PROJECT_NAME}_USE_LOG4CPLUS )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DSTATICLIB_HTTPSERVER_USE_LOG4CPLUS )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -DSTATICLIB_HTTPSERVER_USE_LOG4CPLUS )
endif ( )
if ( ${PROJECT_NAME}_USE_OPENSSL )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DSTATICLIB_HTTPSERVER_HAVE_SSL )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -DSTATICLIB_HTTPSERVER_HAVE_SSL )
endif ( )
if ( ${PROJECT_NAME}_DISABLE_LOGGING ) 
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DSTATICLIB_HTTPSERVER_DISABLE_LOGGING )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -DSTATICLIB_HTTPSERVER_DISABLE_LOGGING )
endif ( )
if ( ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" )
    execute_process( COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE CLANG_FULL_VERSION_STRING )
    string (REGEX REPLACE "^(Apple LLVM version|clang version) ([0-9]+\\.[0-9]+).*" "\\2" CLANG_VERSION_STRING ${CLANG_FULL_VERSION_STRING})
    if ( (APPLE AND (CLANG_VERSION_STRING VERSION_GREATER 6.0)) OR
        ((NOT APPLE) AND (CLANG_VERSION_STRING VERSION_GREATER 3.5)) )
        message ( STATUS "Enabling ASIO warning suppression for Clang: [${CLANG_VERSION_STRING}]" )
        set ( ${PROJECT_NAME}_OPTIONS ${${PROJECT_NAME}_OPTIONS} -Wno-unused-local-typedef )
        set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -Wno-unused-local-typedef )
    endif ( )
endif ( )
if ( ${CMAKE_CXX_COMPILER_ID} MATCHES "GNU" )
    # asio warnings with openssl on
    set ( ${PROJECT_NAME}_OPTIONS ${${PROJECT_NAME}_OPTIONS} -Wno-unused-variable )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -Wno-unused-variable )
endif ( )
set ( ${PROJECT_NAME}_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/asio/asio/include )
target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE 
        ${${PROJECT_NAME}_INCLUDES} 
        ${${PROJECT_NAME}_DEPS_PC_INCLUDE_DIRS} )
target_compile_definitions ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_DEFINITIONS} )
target_compile_options ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_OPTIONS} )
if ( TARGET external_log4cplus )
    add_dependencies ( ${PROJECT_NAME} external_log4cplus )
endif ( )
if ( TARGET external_openssl )
    add_dependencies ( ${PROJECT_NAME} external_openssl )
endif ( )
if ( TARGET external_icu )
    add_dependencies ( ${PROJECT_NAME} external_icu )
endif ( )

# pkg-config
staticlib_httpserver_list_to_string ( ${PROJECT_NAME}_PC_CFLAGS_INC "-I" ${PROJECT_NAME}_INCLUDES )
staticlib_httpserver_list_to_string ( ${PROJECT_NAME}_PC_CFLAGS_OTHER "" ${PROJECT_NAME}_CFLAGS_PUBLIC )
set ( ${PROJECT_NAME}_PC_CFLAGS "${${PROJECT_NAME}_PC_CFLAGS_INC}${${PROJECT_NAME}_PC_CFLAGS_OTHER}" )
set ( ${PROJECT_NAME}_PC_LIBS "-L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -l${PROJECT_NAME}" )
if ( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    set ( ${PROJECT_NAME}_PC_LIBS "${${PROJECT_NAME}_PC_LIBS} -lpthread" )
endif ( )
staticlib_httpserver_list_to_string ( ${PROJECT_NAME}_PC_REQUIRES "" ${PROJECT_NAME}_DEPS )
configure_file ( ${CMAKE_CURRENT_LIST_DIR}/resources/pkg-config.in 
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pkgconfig/${PROJECT_NAME}.pc )
