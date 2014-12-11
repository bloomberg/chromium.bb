// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PORT_H_
#define BASE_PORT_H_

#include <stdarg.h>
#include "base/build_config.h"

#ifdef COMPILER_MSVC
#define GG_LONGLONG(x) x##I64
#define GG_ULONGLONG(x) x##UI64
#else
#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL
#endif

// Per C99 7.8.14, define __STDC_CONSTANT_MACROS before including <stdint.h>
// to get the INTn_C and UINTn_C macros for integer constants.  It's difficult
// to guarantee any specific ordering of header includes, so it's difficult to
// guarantee that the INTn_C macros can be defined by including <stdint.h> at
// any specific point.  Provide GG_INTn_C macros instead.

#define GG_INT8_C(x)    (x)
#define GG_INT16_C(x)   (x)
#define GG_INT32_C(x)   (x)
#define GG_INT64_C(x)   GG_LONGLONG(x)

#define GG_UINT8_C(x)   (x ## U)
#define GG_UINT16_C(x)  (x ## U)
#define GG_UINT32_C(x)  (x ## U)
#define GG_UINT64_C(x)  GG_ULONGLONG(x)

// Define an OS-neutral wrapper for shared library entry points
#if defined(OS_WIN)
#define API_CALL __stdcall
#else
#define API_CALL
#endif

#endif  // BASE_PORT_H_
