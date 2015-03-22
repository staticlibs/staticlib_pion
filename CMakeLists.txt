
cmake_minimum_required ( VERSION 2.8.12 )

# toolchain setup
set ( STATICLIB_TOOLCHAIN linux_amd64_gcc CACHE STRING "toolchain triplet" )
set ( CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/toolchains/${STATICLIB_TOOLCHAIN}.cmake CACHE INTERNAL "" )

# project setup
project ( staticlib_pion_cpp11 CXX )
# common include
include ( ${CMAKE_CURRENT_LIST_DIR}/cmake/staticlibs_common.cmake )
# options, use SET ( OPTNAME ON CACHE BOOL "") to override
option ( STATICLIB_PION_USE_LOG4CPLUS "Use log4cplus" OFF )
option ( STATICLIB_PION_USE_OPENSSL "Use OpenSSL" OFF )

# build steps
set ( ${PROJECT_NAME}_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/deps/asio/asio/include )
set ( ${PROJECT_NAME}_DEFINITIONS
        -DASIO_STANDALONE
        -DASIO_HAS_STD_CHRONO )
if ( STATICLIB_TOOLCHAIN MATCHES "windows_[^_]+_msvc" )    
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS}
            -D_CRT_SECURE_NO_WARNINGS
            -D_SCL_SECURE_NO_WARNINGS
            -D_WIN32_WINNT=0x0501 
            -DPION_CMAKE_BUILD
            -DPION_STATIC_LINKING )
endif ( )
if ( STATICLIB_PION_USE_LOG4CPLUS )
    staticlib_add_subdirectory ( external_log4cplus )
    set ( ${PROJECT_NAME}_INCLUDES ${${PROJECT_NAME}_INCLUDES} ${external_log4cplus_INCLUDES} )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_STATICLIB_PION_USE_LOG4CPLUS )
endif ( )
if ( STATICLIB_PION_USE_OPENSSL )
    staticlib_add_subdirectory ( external_openssl )
    set ( ${PROJECT_NAME}_INCLUDES ${${PROJECT_NAME}_INCLUDES} ${external_openssl_INCLUDES} )
    set ( ${PROJECT_NAME}_DEFINITIONS ${${PROJECT_NAME}_DEFINITIONS} -DPION_HAVE_SSL )
endif ( )
# build library
file ( GLOB_RECURSE ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp )
add_library ( ${PROJECT_NAME} STATIC ${${PROJECT_NAME}_SRC} )
target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE ${${PROJECT_NAME}_INCLUDES} )
target_compile_definitions ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_DEFINITIONS} )
# force building openssl first
if ( STATICLIB_PION_USE_OPENSSL )
    add_dependencies ( ${PROJECT_NAME} external_openssl )
endif ( )

# tests
enable_testing ( )
add_executable ( pion_test ${CMAKE_CURRENT_LIST_DIR}/test/pion_test.cpp )
target_include_directories ( pion_test BEFORE PRIVATE ${${PROJECT_NAME}_INCLUDES} )
target_link_libraries ( pion_test ${PROJECT_NAME} )
if ( STATICLIB_TOOLCHAIN MATCHES "linux_[^_]+_[^_]+" )
    target_link_libraries ( pion_test -pthread )
endif ( )
if ( STATICLIB_PION_USE_LOG4CPLUS )
    target_link_libraries ( pion_test external_log4cplus )
endif ( )
if ( STATICLIB_PION_USE_OPENSSL )
    target_link_libraries ( pion_test external_openssl external_openssl_ssl )
endif ( )
target_compile_definitions ( pion_test PRIVATE ${${PROJECT_NAME}_DEFINITIONS} )
if ( DEFINED CMAKE_MEMORYCHECK_COMMAND )
    add_test ( pion_test 
            ${CMAKE_MEMORYCHECK_COMMAND} ${CMAKE_MEMORYCHECK_COMMAND_OPTIONS} 
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pion_test )
else ( )
    add_test ( pion_test ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pion_test )
endif ( )

# docs
staticlib_enable_docs ( )

# export headers
set ( ${PROJECT_NAME}_INCLUDES ${${PROJECT_NAME}_INCLUDES} CACHE INTERNAL "" )
