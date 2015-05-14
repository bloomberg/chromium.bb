// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define a set of C++ specific macros.
// Mojo C++ API users can assume that mojo/public/cpp/system/macros.h
// includes mojo/public/c/system/macros.h.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_MACROS_H_
#define MOJO_PUBLIC_CPP_SYSTEM_MACROS_H_

#include "mojo/public/c/system/macros.h"  // Symbols exposed.

// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
#define MOJO_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)

// Used to calculate the number of elements in an array.
// (See |arraysize()| in Chromium's base/basictypes.h for more details.)
namespace mojo {
template <typename T, size_t N>
char(&ArraySizeHelper(T(&array)[N]))[N];
#if !defined(_MSC_VER)
template <typename T, size_t N>
char(&ArraySizeHelper(const T(&array)[N]))[N];
#endif
}  // namespace mojo
#define MOJO_ARRAYSIZE(array) (sizeof(::mojo::ArraySizeHelper(array)))

// Used to make a type move-only. See Chromium's base/move.h for more
// details. The MoveOnlyTypeForCPP03 typedef is for Chromium's base/callback to
// tell that this type is move-only.
#define MOJO_MOVE_ONLY_TYPE(type)                                              \
 private:                                                                      \
  type(type&);                                                                 \
  void operator=(type&);                                                       \
                                                                               \
 public:                                                                       \
  type&& Pass() MOJO_WARN_UNUSED_RESULT { return static_cast<type&&>(*this); } \
  typedef void MoveOnlyTypeForCPP03;                                           \
                                                                               \
 private:

// The C++ standard requires that static const members have an out-of-class
// definition (in a single compilation unit), but MSVC chokes on this (when
// language extensions, which are required, are enabled). (You're only likely to
// notice the need for a definition if you take the address of the member or,
// more commonly, pass it to a function that takes it as a reference argument --
// probably an STL function.) This macro makes MSVC do the right thing. See
// http://msdn.microsoft.com/en-us/library/34h23df8(v=vs.100).aspx for more
// information. Use like:
//
// In .h file:
//   struct Foo {
//     static const int kBar = 5;
//   };
//
// In .cc file:
//   STATIC_CONST_MEMBER_DEFINITION const int Foo::kBar;
#if defined(_MSC_VER)
#define MOJO_STATIC_CONST_MEMBER_DEFINITION __declspec(selectany)
#else
#define MOJO_STATIC_CONST_MEMBER_DEFINITION
#endif

#endif  // MOJO_PUBLIC_CPP_SYSTEM_MACROS_H_
