Staticlibs reflection library
=============================

This project is a part of [Staticlibs](http://staticlibs.net/).

TODO

Link to [API documentation TODO](#).

How to build
------------

[CMake](http://cmake.org/) is required for building.

Staticlib toolchain name must be specified as a `STATICLIB_TOOLCHAIN` parameter to `cmake` command
unless you are using GCC on Linux x86_64 (`linux_amd64_gcc` toolchain) that is default.

Example build for Windows x86_64 with Visual Studio 2013 Express, run the following commands 
from the development shell `C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\Shortcuts\VS2013 x64 Cross Tools Command Prompt` :

    git clone https://github.com/staticlibs/staticlib_TODO.git
    cd staticlib_TODO
    git submodule update --init --recursive
    mkdir build
    cd build
    cmake .. -DSTATICLIB_TOOLCHAIN=windows_amd64_msvc -G "NMake Makefiles"
    nmake
    nmake test

Build with log4cplust and OpenSSL support (run from project root):

    git clone --branch 1.0.2 https://github.com/staticlibs/external_openssl.git deps/external_openssl
    cd deps/external_openssl/
    git submodule update --init --recursive
    cd ../..
    git clone --branch 1.1.2 https://github.com/staticlibs/external_log4cplus.git deps/external_log4cplus
    cd deps/external_log4cplus/
    git submodule update --init --recursive
    cd ../..
    mkdir build
    cd build
    cmake .. -DSTATICLIB_PION_USE_LOG4CPLUS=ON -DSTATICLIB_PION_USE_OPENSSL=ON -DSTATICLIB_TOOLCHAIN=...

License information
-------------------

This project is released under the [Apache License 2.0](http://www.apache.org/licenses/LICENSE-2.0)

Changelog
---------

**2015-06-04**

 * 5.0.7, initial public version
