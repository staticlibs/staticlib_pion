Staticlibs HTTP Server library
==============================

This project is a part of [Staticlibs](http://staticlibs.net/).

Embedded asynchronous HTTP 1.1 server implementation based on a source code from [Pion HTTP server](https://github.com/splunk/pion)
project ([description](http://sourceforge.net/p/pion/mailman/message/32075645/)) with the following changes:

 - all Boost dependencies removed in favour of C++11 with [standalone Asio](https://think-async.com/Asio/AsioStandalone)
 - some optional non-HTTP functionality removed (dynamic-load plugins, SPDY support)
 - added support for streaming requests  of arbitrary size (file upload)
 - warnings cleanup to compile with `/W4 /WX` on MSVC and with `-Wall -Werror -Wextra` on GCC and Clang; Doxygen comments reformatted

All changes were done with an attempt to stay as close as possible to the original Pion implementation.

Link to [API documentation](http://staticlibs.github.io/staticlib_httpserver/docs/html/namespacepion_1_1http.html).

Usage [example](https://github.com/staticlibs/staticlib_httpserver/blob/master/test/pion_test.cpp).

How to build
------------

[CMake](http://cmake.org/) is required for building.

Staticlib toolchain name must be specified as a `STATICLIB_TOOLCHAIN` parameter to `cmake` command
unless you are using GCC on Linux x86_64 (`linux_amd64_gcc` toolchain) that is a default one.

List of [supported toolchains](https://github.com/staticlibs/cmake/tree/master/toolchains).

Example build for Windows x86_64 with Visual Studio 2013 Express, run the following commands 
from the development shell `C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\Shortcuts\VS2013 x64 Cross Tools Command Prompt` :

    git clone https://github.com/staticlibs/staticlib_httpserver.git
    cd staticlib_httpserver
    git submodule update --init --recursive
    mkdir build
    cd build
    cmake .. -DSTATICLIB_TOOLCHAIN=windows_amd64_msvc -G "NMake Makefiles"
    nmake
    nmake test

To build with system OpenSSL and log4cplus libraries use the followin CMake switches:

    cmake .. -DSTATICLIB_PION_USE_SYSTEM_LOG4CPLUS=ON -DSTATICLIB_PION_USE_SYSTEM_OPENSSL=ON -DSTATICLIB_TOOLCHAIN=...

For platforms without pre-existing OpenSSL and log4cplus you can build this project with Staticlibs 
[log4cplus](https://github.com/staticlibs/external_log4cplus) and 
[OpenSSL](https://github.com/staticlibs/external_openssl) libs the following way (run from project root):

    git clone --branch 1.1.2.2 https://github.com/staticlibs/external_log4cplus.git deps/external_log4cplus
    cd deps/external_log4cplus/
    git submodule update --init --recursive
    cd ../..
    git clone --branch 1.0.2.2 https://github.com/staticlibs/external_openssl.git deps/external_openssl
    cd deps/external_openssl/
    git submodule update --init --recursive
    cd ../..
    mkdir build
    cd build
    cmake .. -DSTATICLIB_PION_USE_STATICLIB_LOG4CPLUS=ON -DSTATICLIB_PION_USE_STATICLIB_OPENSSL=ON -DSTATICLIB_TOOLCHAIN=...

License information
-------------------

This project is released under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0)

Changelog
---------

**2015-09-05**

 * version 5.0.7.5
 * minor build changes

**2015-07-12**

 * version 5.0.7.4
 * support for HTTPS with client certificate auth to `streaming_server`

**2015-07-05**

 * version 5.0.7.3
 * toolchains update
 * clang 3.6+ fix with ASIO

**2015-07-02**

 * version 5.0.7.2
 * locking fixes (use `lock_guards`)
 * suppress "no payload handler for GET" log warning

**2015-07-01**

 * version 5.0.7.1 - toolchains update, minor cmake fixes

**2015-06-04**

 * 5.0.7, initial public version
