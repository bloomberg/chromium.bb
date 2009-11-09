/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * This file should be at the top of the #include group, followed by
 * standard system #include files, then by native client specific
 * includes.
 *
 * TODO(gregoryd): explain why.  (Something to do with windows include
 * files, to be reconstructed.)
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_H_
#define NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_H_ 1

#include <stdlib.h>

#include "native_client/src/include/nacl_base.h"
#ifdef __native_client__
#include <bits/wordsize.h>
#else
#include "native_client/src/trusted/service_runtime/include/bits/wordsize.h"
#endif

#if NACL_WINDOWS
/* disable warnings for deprecated functions like getenv, etc. */
#pragma warning( disable : 4996)
# include <malloc.h>
/* TODO: eliminate port_win.h */
# include "native_client/src/include/win/port_win.h"
#else
# include <sys/types.h>
# include <stdint.h>
# include <unistd.h>
# include <sys/time.h>
#endif  /*NACL_WINDOWS*/

/* MSVC supports "inline" only in C++ */
#if NACL_WINDOWS
# define INLINE __inline
#else
# if __GNUC_MINOR__ >= 2
#  define INLINE __inline__ __attribute__((gnu_inline))
# else
#  define INLINE __inline__
# endif
#endif

#if NACL_WINDOWS
# define DLLEXPORT __declspec(dllexport)
#elif NACL_OSX
# define DLLEXPORT
#elif  NACL_LINUX
# define DLLEXPORT __attribute__ ((visibility("default")))
#elif defined(__native_client__)
/* do nothing */
#else
# error "what platform?"
#endif

#if NACL_WINDOWS
# define ATTRIBUTE_FORMAT_PRINTF(m, n)
#else
# define ATTRIBUTE_FORMAT_PRINTF(m, n) __attribute__((format(printf, m, n)))
#endif

#if NACL_WINDOWS
# define UNREFERENCED_PARAMETER(P) (P)
#else
# define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif

#if NACL_WINDOWS
# define NORETURN __declspec(noreturn)
#else
# define NORETURN __attribute__((noreturn))  /* assumes gcc */
# define _cdecl /* empty */
#endif

#if NACL_WINDOWS
# define THREAD __declspec(thread)
#else
# define THREAD __thread
# define WINAPI
#endif

#if NACL_WINDOWS
# define NACL_WUR
#else
# define NACL_WUR __attribute__((__warn_unused_result__))
#endif

/*
 * Per C99 7.8.14, define __STDC_CONSTANT_MACROS before including <stdint.h>
 * to get the INTn_C and UINTn_C macros for integer constants.  It's difficult
 * to guarantee any specific ordering of header includes, so it's difficult to
 * guarantee that the INTn_C macros can be defined by including <stdint.h> at
 * any specific point.  Provide GG_INTn_C macros instead.
 */

#define GG_INT8_C(x)    (x)
#define GG_INT16_C(x)   (x)
#define GG_INT32_C(x)   (x)
#define GG_INT64_C(x)   GG_LONGLONG(x)

#define GG_UINT8_C(x)   (x ## U)
#define GG_UINT16_C(x)  (x ## U)
#define GG_UINT32_C(x)  (x ## U)
#define GG_UINT64_C(x)  GG_ULONGLONG(x)

#if NACL_WINDOWS
# define LOCALTIME_R(in_time_t_ptr, out_struct_tm_ptr) \
  (0 == localtime_s(out_struct_tm_ptr, in_time_t_ptr)                   \
   ? out_struct_tm_ptr : (struct tm *) 0)  /* NULL requires stdio.h */

struct timezone {
  int tz_minuteswest;
  int tz_dsttime;
};

#else

# define LOCALTIME_R(in_time_t_ptr, out_struct_tm_ptr) \
  localtime_r(in_time_t_ptr, out_struct_tm_ptr)
#endif

/*
 * printf macros for size_t, in the style of inttypes.h.  this is
 * needed since the windows compiler does not understand %zd
 * etc. 64-bit windows uses 32-bit long and does not include long
 * long
 */
#if NACL_WINDOWS
# if defined(__x86_64__)
#  define  __PRIS_PREFIX "I64"
# else
#  define  __PRIS_PREFIX
# endif
#else
# if defined(__STRICT_ANSI__)
#  if NACL_OSX
#   define  __PRIS_PREFIX "l"
#  else
#   if __WORDSIZE == 64
#    define __PRIS_PREFIX "l"
#   else
#    define  __PRIS_PREFIX
#   endif
#  endif
# else
#  if NACL_OSX
#   define  __PRIS_PREFIX "l" /* -pedantic C++ programs w/ xcode */
#  else
#   define  __PRIS_PREFIX "z"
#  endif
# endif
#endif

#define PRIdS __PRIS_PREFIX "d"
#define PRIiS __PRIS_PREFIX "i"
#define PRIoS __PRIS_PREFIX "o"
#define PRIuS __PRIS_PREFIX "u"
#define PRIxS __PRIS_PREFIX "x"
#define PRIXS __PRIS_PREFIX "X"

/*
 * printf macros for intptr_t and uintptr_t, int{8,16,32,64}
 */
#if !NACL_WINDOWS
#ifndef __STDC_FORMAT_MACROS
# define __STDC_FORMAT_MACROS  /* C++ */
#endif
# include <inttypes.h>
# if NACL_OSX
/*
 * OSX defines "hh" prefix for int8_t etc, but that's not standards
 * compliant -- --std=c++98 -Wall -Werror rejects it.
 */
#  undef PRId8
#  undef PRIi8
#  undef PRIo8
#  undef PRIu8
#  undef PRIx8
#  undef PRIX8
#  define PRId8  "d"
#  define PRIi8  "i"
#  define PRIo8  "o"
#  define PRIu8  "u"
#  define PRIx8  "x"
#  define PRIX8  "X"
# endif
#else
# define __PRIPTR_PREFIX "l"
# define PRIdPTR __PRIPTR_PREFIX "d"
# define PRIiPTR __PRIPTR_PREFIX "i"
# define PRIoPTR __PRIPTR_PREFIX "o"
# define PRIuPTR __PRIPTR_PREFIX "u"
# define PRIxPTR __PRIPTR_PREFIX "x"
# define PRIXPTR __PRIPTR_PREFIX "X"

# define PRId8  "d"
# define PRIi8  "i"
# define PRIo8  "o"
# define PRIu8  "u"
# define PRIx8  "x"
# define PRIX8  "X"

# define PRId16 "d"
# define PRIi16 "i"
# define PRIo16 "o"
# define PRIu16 "u"
# define PRIx16 "x"
# define PRIX16 "X"

# define __PRI32_PREFIX "I32"

# define PRId32 __PRI32_PREFIX "d"
# define PRIi32 __PRI32_PREFIX "i"
# define PRIo32 __PRI32_PREFIX "o"
# define PRIu32 __PRI32_PREFIX "u"
# define PRIx32 __PRI32_PREFIX "x"
# define PRIX32 __PRI32_PREFIX "X"

# define __PRI64_PREFIX "I64"

# define PRId64 __PRI64_PREFIX "d"
# define PRIi64 __PRI64_PREFIX "i"
# define PRIo64 __PRI64_PREFIX "o"
# define PRIu64 __PRI64_PREFIX "u"
# define PRIx64 __PRI64_PREFIX "x"
# define PRIX64 __PRI64_PREFIX "X"

#endif

#endif  /* NATIVE_CLIENT_SRC_INCLUDE_PORTABILITY_H_ */
