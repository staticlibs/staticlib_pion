Pion HTTP Server build for Staticlibs
=====================================

[![travis](https://travis-ci.org/staticlibs/staticlib_pion.svg?branch=master)](https://travis-ci.org/staticlibs/staticlib_pion)
[![appveyor](https://ci.appveyor.com/api/projects/status/github/staticlibs/staticlib_pion?svg=true)](https://ci.appveyor.com/project/staticlibs/staticlib-pion)

This project is a part of [Staticlibs](http://staticlibs.net/).

Embedded asynchronous HTTP 1.1 server implementation based on a source code from [Pion HTTP server](https://github.com/splunk/pion)
project ([description](http://sourceforge.net/p/pion/mailman/message/32075645/)) that was heavily modified
to support WebSocket and CORS.

Link to the [API documentation](http://staticlibs.github.io/staticlib_pion/docs/html/namespacestaticlib_1_1pion.html).

Usage [example](https://github.com/staticlibs/staticlib_pion/blob/master/test/pion_test.cpp).

How to build
------------

[CMake](http://cmake.org/) is required for building.

[pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/) utility is used for dependency management.
For Windows users ready-to-use binary version of `pkg-config` can be obtained from [tools_windows_pkgconfig](https://github.com/staticlibs/tools_windows_pkgconfig) repository.
See [StaticlibsPkgConfig](https://github.com/staticlibs/wiki/wiki/StaticlibsPkgConfig) for Staticlibs-specific details about `pkg-config` usage.

[Perl](https://www.perl.org/) is also required for building, Windows users can obtain ready-to-use
Perl distribution from [tools_windows_perl](https://github.com/staticlibs/tools_windows_perl) repository.

[NASM](http://nasm.us/) is also required for building on Windows x86 
(optional on Windows x86_64 - will be used if present in `PATH`).
You can obtain ready-to-use NASM distribution from 
[tools_windows_nasm](https://github.com/staticlibs/tools_windows_nasm) repository.

To build the library on Windows using Visual Studio 2013 Express run the following commands using
Visual Studio development command prompt 
(`C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\Shortcuts\VS2013 x86 Native Tools Command Prompt`):

    git clone https://github.com/staticlibs/tools_windows_pkgconfig.git
    git clone https://github.com/staticlibs/tools_windows_perl520.git
    git clone https://github.com/staticlibs/tools_windows_nasm.git
    set PATH=%PATH%;c:\projects\tools_windows_pkgconfig\bin;c:\projects\tools_windows_perl520\perl\bin;c:\projects\tools_windows_nasm
    git clone https://github.com/staticlibs/cmake.git
    git clone https://github.com/staticlibs/external_asio.git
    git clone https://github.com/staticlibs/lookaside_asio.git
    git clone https://github.com/staticlibs/external_openssl.git
    git clone https://github.com/staticlibs/lookaside_openssl.git
    git clone https://github.com/staticlibs/staticlib_config.git
    git clone https://github.com/staticlibs/staticlib_support.git
    git clone https://github.com/staticlibs/staticlib_io.git
    git clone https://github.com/staticlibs/staticlib_endian.git
    git clone https://github.com/staticlibs/staticlib_crypto.git
    git clone https://github.com/staticlibs/staticlib_websocket.git
    git clone https://github.com/staticlibs/staticlib_utils.git
    git clone https://github.com/staticlibs/staticlib_pion.git
    cd staticlib_pion
    mkdir build
    cd build
    cmake ..
    msbuild staticlib_pion.sln

To build on other platforms using GCC or Clang with GNU Make:

    cmake .. -DCMAKE_CXX_FLAGS="--std=c++11"
    make

Cloning of `external_asio` and tools is not required on Linux - system libraries will be used instead.

See [StaticlibsToolchains](https://github.com/staticlibs/wiki/wiki/StaticlibsToolchains) for 
more information about the toolchain setup and cross-compilation.

License information
-------------------

This project is released under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0).

Changelog
---------

**2018-10-27**

 * version 2.0.2
 * do not send `500` response on handler exception

**2018-06-21**

 * version 2.0.1
 * less strict checking for WebSocket handshake

**2018-06-20**

 * version 2.0.0
 * core networking rework, version reset to untie from upstream pion versions
 * WebSocket support

**2018-05-21**

 * version 5.0.7-17
 * make thread-stop hooks to run from the same thread

**2018-03-12**

 * version 5.0.7-16
 * support logging through `wilton_logging`

**2017-12-25**

 * version 5.0.7-15
 * vs2017 support

**2017-05-19**

 * version 5.0.7-14
 * rename back to `pion`

**2016-10-29**

 * version 5.0.7-13
 * use `asio` submodule from Fedora EPEL lookaside - it is thinner than upstream one and pre-patched for android_i386 and musl libc

**2016-07-14**

 * version 5.0.7-12
 * `HEAD` and `OPTIONS` methods support

**2016-07-12**

 * version 5.0.7-11
 * cleanup type aliases usage
 * replace `std::bind` with lambdas
 * minor `http_response_writer` API cleanup

**2016-03-09**

 * version 5.0.7-10
 * major namespaces and classes renaming
 * original Pion `http::server` removed in favour of a `streaming` one

**2016-01-22**

 * version 5.0.7-9
 * minor CMake changes

**2015-12-03**

 * version 5.0.7-8
 * fix type in cmake workaround for newer clang

**2015-12-03**

 * version 5.0.7-7
 * deplibs cache support
 * minor cmake cleanup

**2015-10-20**

 * version 5.0.7-6
 * `pkg-config` integration
 * filters support
 * `shared_ptr` cleanup
 * `streaming_server` API changes
 * client certificate support fix

**2015-09-05**

 * version 5.0.7-5
 * minor build changes

**2015-07-12**

 * version 5.0.7-4
 * support for HTTPS with client certificate auth to `streaming_server`

**2015-07-05**

 * version 5.0.7-3
 * toolchains update
 * clang 3.6+ fix with ASIO

**2015-07-02**

 * version 5.0.7-2
 * locking fixes (use `lock_guards`)
 * suppress "no payload handler for GET" log warning

**2015-07-01**

 * version 5.0.7-1 - toolchains update, minor cmake fixes

**2015-06-04**

 * 5.0.7, initial public version
