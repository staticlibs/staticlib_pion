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
include ( ${CMAKE_CURRENT_LIST_DIR}/resources/macros.cmake )

# project
project ( staticlib_httpserver CXX )
set ( ${PROJECT_NAME}_STATICLIB_VERSION 5.0.7.6 )
set ( ${PROJECT_NAME}_DESCRIPTION "Embedded async HTTP 1.1 server based on a source code from https://github.com/splunk/pion" )
set ( ${PROJECT_NAME}_URL https://github.com/staticlibs/staticlib_httpserver )

# docs
option ( ${PROJECT_NAME}_ENABLE_DOCS "Generate doxyfile and exit build" OFF )
if ( ${PROJECT_NAME}_ENABLE_DOCS )
    configure_file ( ${CMAKE_CURRENT_LIST_DIR}/resources/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile )
    return ( )
endif ( )

# options, use SET ( OPTNAME ON CACHE BOOL "") in parent to override
option ( ${PROJECT_NAME}_DISABLE_LOGGING "Disable logging to std out and err" OFF )
option ( ${PROJECT_NAME}_USE_LOG4CPLUS "Use log4cplus lib for logging" OFF )
option ( ${PROJECT_NAME}_USE_OPENSSL "Use OpenSSL lib for https" OFF )

# dependencies
set ( ${PROJECT_NAME}_DEP_MODULES "" )
if ( ${PROJECT_NAME}_USE_LOG4CPLUS )
    set ( ${PROJECT_NAME}_DEP_MODULES ${${PROJECT_NAME}_DEP_MODULES} log4cplus )
endif ( )
if ( ${PROJECT_NAME}_USE_OPENSSL )
    set ( ${PROJECT_NAME}_DEP_MODULES ${${PROJECT_NAME}_DEP_MODULES} openssl )
endif ( )
staticlib_pkg_check_modules ( ${PROJECT_NAME}_DEP_PC REQUIRED ${PROJECT_NAME}_DEP_MODULES )

# library
file ( GLOB_RECURSE ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp )
add_library ( ${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRC} )
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
            -DPION_CMAKE_BUILD
            -DPION_STATIC_LINKING )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -D_WIN32_WINNT=0x0501 -DPION_STATIC_LINKING )
endif ( )
if ( ${PROJECT_NAME}_USE_LOG4CPLUS )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_USE_LOG4CPLUS )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -DPION_USE_LOG4CPLUS )
endif ( )
if ( ${PROJECT_NAME}_USE_OPENSSL )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_HAVE_SSL )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -DPION_HAVE_SSL )
endif ( )
if ( ${PROJECT_NAME}_DISABLE_LOGGING ) 
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_DISABLE_LOGGING )
    set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${${PROJECT_NAME}_CFLAGS_PUBLIC} -DPION_DISABLE_LOGGING )
endif ( )
if ( ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" )
    EXECUTE_PROCESS( COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE clang_full_version_string )
    string (REGEX REPLACE ".*clang version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${clang_full_version_string})
    if ( CLANG_VERSION_STRING VERSION_GREATER 3.5 )
        set ( ${PROJECT_NAME}_OPTIONS ${${PROJECT_NAME}_OPTIONS} -Wno-unused-local-typedef )
        set ( ${PROJECT_NAME}_CFLAGS_PUBLIC ${ ${PROJECT_NAME}_CFLAGS_PUBLIC} -Wno-unused-local-typedef )
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
        ${${PROJECT_NAME}_DEP_PC_INCLUDE_DIRS} )
target_compile_definitions ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_DEFINITIONS} )
target_compile_options ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_OPTIONS} )

# pkg-config
if ( NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY )
    set ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
endif ( )
staticlib_list_to_string ( ${PROJECT_NAME}_PC_CFLAGS_INC "-I" ${PROJECT_NAME}_INCLUDES )
staticlib_list_to_string ( ${PROJECT_NAME}_PC_CFLAGS_OTHER "" ${PROJECT_NAME}_CFLAGS_PUBLIC )
set ( ${PROJECT_NAME}_PC_CFLAGS "${${PROJECT_NAME}_PC_CFLAGS_INC}${${PROJECT_NAME}_PC_CFLAGS_OTHER}" )
set ( ${PROJECT_NAME}_PC_LIBS "-L${CMAKE_LIBRARY_OUTPUT_DIRECTORY} -l${PROJECT_NAME}" )
if ( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
    set ( ${PROJECT_NAME}_PC_LIBS "${${PROJECT_NAME}_PC_LIBS} -lpthread" )
endif ( )
staticlib_list_to_string ( ${PROJECT_NAME}_PC_REQUIRES "" ${PROJECT_NAME}_DEP_MODULES )
configure_file ( ${CMAKE_CURRENT_LIST_DIR}/resources/pkg-config.in 
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pkgconfig/${PROJECT_NAME}.pc )
