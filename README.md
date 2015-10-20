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

To build the library on Windows using Visual Studio 2013 Express run the following commands using
Visual Studio development command prompt 
(`C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\Shortcuts\VS2013 x86 Native Tools Command Prompt`):

    git clone --recursive https://github.com/staticlibs/staticlib_httpserver.git
    cd staticlib_httpserver
    mkdir build
    cd build
    cmake ..
    msbuild staticlib_httpserver.sln

To build on other platforms using GCC or Clang with GNU Make:

    cmake .. -DCMAKE_CXX_FLAGS="--std=c++11"
    make

See [StaticlibsToolchains](https://github.com/staticlibs/wiki/wiki/StaticlibsToolchains) for 
more information about the toolchain setup and cross-compilation.

Build with HTTPS and logging support
------------------------------------

This project has optional support for `OpenSSL` library to enable HTTPS and for `log4cplus` library
for logging. On Linux development versions of these libraries must be installed. 
For other platforms - see notes about `pkg-config` below. To configure build with these dependencies run:

    cmake .. -Dstaticlib_httpserver_USE_LOG4CPLUS=ON -Dstaticlib_httpserver_USE_OPENSSL=ON

[pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/) utility is used for dependency management.
For Windows users ready-to-use binary version of `pkg-config` can be obtained from [tools_windows_pkgconfig](https://github.com/staticlibs/tools_windows_pkgconfig) repository.

See [PkgConfig](https://github.com/staticlibs/wiki/wiki/PkgConfig) for Staticlibs-specific details about `pkg-config` usage.

To build this project with `OpenSSL` and `log4cplus` support manually on
platforms without corresponding system libraries you can use 
[external_openssl](https://github.com/staticlibs/external_openssl) and 
[external_log4cplus](https://github.com/staticlibs/external_log4cplus) projects: 

 * checkout both dependent projects (using `--recursive` flag)
 * configure these projects using the same output directory (see their readmes for specific build instructions):

Run:

    mkdir build
    cd build
    cmake .. -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=<my_lib_dir>

 * build all the dependent projects
 * configure this projects using the same output directory and build it

See [StaticlibsDependencies](https://github.com/staticlibs/wiki/wiki/StaticlibsDependencies) for more 
details about dependency management with Staticlibs.

License information
-------------------

This project is released under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0).

Changelog
---------

**2015-10-20**

 * version 5.0.7.6
 * `pkg-config` integration
 * filters support
 * `shared_ptr` cleanup
 * `streaming_server` API changes
 * client certificate support fix

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
