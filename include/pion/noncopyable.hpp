//  Boost noncopyable.hpp header file  --------------------------------------//

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

namespace noncopyable_  // protection from unintended ADL
{
  class noncopyable
  {
  protected:
      noncopyable() = default;
      ~noncopyable() = default;

      noncopyable( const noncopyable& ) = delete;
      noncopyable& operator=( const noncopyable& ) = delete;
  };
}

typedef noncopyable_::noncopyable noncopyable;

} // namespace

#endif  // __PION_NONCOPYABLE_HPP__
