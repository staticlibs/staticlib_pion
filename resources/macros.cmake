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

# enhanced version of pkg_check_modules macro with PKG_CONFIG_PATH logic
# PKG_CONFIG_PATH handling through CMAKE_PREFIX_PATH was added in newer versions of CMake
macro ( ${PROJECT_NAME}_pkg_check_modules _out_var_name _modifier _modules_list_var_name )
    find_package ( PkgConfig )
    if ( WIN32 )
        set ( PATHENV_SEPARATOR ";" )
    else ( )
        set ( PATHENV_SEPARATOR ":" )
    endif ( )
    set (_pkgconfig_path $ENV{PKG_CONFIG_PATH} )
    if ( STATICLIB_USE_DEPLIBS_CACHE )
        set ( ENV{PKG_CONFIG_PATH} "${STATICLIB_DEPLIBS_CACHE_DIR}/pkgconfig${PATHENV_SEPARATOR}$ENV{PKG_CONFIG_PATH}" )
    endif ( )
    set ( ENV{PKG_CONFIG_PATH} "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pkgconfig${PATHENV_SEPARATOR}$ENV{PKG_CONFIG_PATH}" )
    pkg_check_modules ( ${_out_var_name} ${_modifier} ${${_modules_list_var_name}} )
    set ( ENV{PKG_CONFIG_PATH} ${_pkgconfig_path} )
endmacro ( )

# converts list to space-separated string with a prefix to each element
macro ( ${PROJECT_NAME}_list_to_string _out_var_name _prefix _list_var_name )
    set ( ${_out_var_name} "" )
    foreach ( _el ${${_list_var_name}} )
        set ( ${_out_var_name} "${${_out_var_name}}${_prefix}${_el} " )
    endforeach ( )
endmacro ( )
