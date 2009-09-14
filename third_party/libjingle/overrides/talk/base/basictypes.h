// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file overrides the inclusion of talk/base/basictypes.h to remove
// collisions with Chromium's base/basictypes.h.   We then add back a few
// items that Chromium's version doesn't provide, but libjingle expects.

#ifndef OVERRIDES_TALK_BASE_BASICTYPES_H__
#define OVERRIDES_TALK_BASE_BASICTYPES_H__

#include "base/basictypes.h"

#ifndef INT_TYPES_DEFINED
#define INT_TYPES_DEFINED
#ifdef COMPILER_MSVC
typedef __int64 int64;
#else
typedef long long int64;
#endif /* COMPILER_MSVC */

#ifdef COMPILER_MSVC
typedef unsigned __int64 uint64;
typedef __int64 int64;
#define INT64_C(x) x ## I64
#define UINT64_C(x) x ## UI64
#define INT64_F "I64"
#else
typedef unsigned long long uint64;
typedef long long int64;
#define INT64_C(x) x ## LL
#define UINT64_C(x) x ## ULL
#define INT64_F "ll"
#endif /* COMPILER_MSVC */
#endif  // INT_TYPES_DEFINED

#ifdef WIN32
typedef int socklen_t;
#endif

namespace talk_base {
template<class T> inline T _min(T a, T b) { return (a > b) ? b : a; }
template<class T> inline T _max(T a, T b) { return (a < b) ? b : a; }
}

#endif // OVERRIDES_TALK_BASE_BASICTYPES_H__
