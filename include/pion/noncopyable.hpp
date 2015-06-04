/*
 * Copyright 2015, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//  (C) Copyright Beman Dawes 1999-2003. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/utility for documentation.

#ifndef __PION_NONCOPYABLE_HPP__
#define __PION_NONCOPYABLE_HPP__

namespace pion {

//  Private copy constructor and copy assignment ensure classes derived from
//  class noncopyable cannot be copied.

//  Contributed by Dave Abrahams

/**
 * Internal namespace for noncopyable for protection from unintended ADL
 */
namespace noncopyable_  // protection from unintended ADL
{
  class noncopyable
  {
  protected:
      /**
       * Defaulted constructor
       */
      noncopyable() = default;
      
      /**
       * Defaulted destructor
       */
      ~noncopyable() = default;
      
      /**
       * Deleted copy constructor
       * 
       * @param a another instance
       */
      noncopyable( const noncopyable& ) = delete;
      
      /**
       * Deleted copy assignment operator
       * 
       * @param a another instance
       * @return new instance
       */
      noncopyable& operator=( const noncopyable& ) = delete;
  };
}

/**
 * Import noncopyable into pion namespace
 */
typedef noncopyable_::noncopyable noncopyable;

} // end namespace pion

#endif  // __PION_NONCOPYABLE_HPP__
