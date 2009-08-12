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
 * NaCl service runtime, check macros.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CHECK_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CHECK_H_

#include "native_client/src/shared/platform/nacl_log.h"

/*
 * We cannot use variadic macros since not all preprocessors provide
 * them.  Instead, we require uses of the CHECK and DCHECK macro write
 * code in the following manner:
 *
 *   CHECK(a == b);
 *
 * or
 *
 *   VCHECK(a == b, ("a = %d, b = %d, c = %s\n", a, b, some_cstr));
 *
 * depending on whether a printf-like, more detailed message in
 * addition to the invariance failure should be printed.
 *
 * NB: BEWARE of printf arguments the evaluation of which have
 * side-effects.  Any such will cause the program to behave
 * differently depending on whether debug mode is enabled or not.
 */
#define CHECK(bool_expr) do {                                        \
    if (!(bool_expr)) {                                              \
      NaClLog(LOG_FATAL, "Fatal error in file %s, line %d: !(%s)\n", \
              __FILE__, __LINE__, #bool_expr);                       \
    }                                                                \
  } while (0)

#define DCHECK(bool_expr) do {                                       \
    if (nacl_check_debug_mode && !(bool_expr)) {                     \
      NaClLog(LOG_FATAL, "Fatal error in file %s, line %d: !(%s)\n", \
              __FILE__, __LINE__, #bool_expr);                       \
    }                                                                \
  } while (0)

#define VCHECK(bool_expr, fn_arg) do {                               \
    if   (!(bool_expr)) {                                            \
      NaClLog(LOG_ERROR, "Fatal error in file %s, line %d: !(%s)\n", \
              __FILE__, __LINE__, #bool_expr);                       \
      NaClCheckIntern fn_arg;                                        \
    }                                                                \
  } while (0)

#define DVCHECK(bool_expr, fn_arg) do {                              \
    if (nacl_check_debug_mode && !(bool_expr)) {                     \
      NaClLog(LOG_ERROR, "Fatal error in file %s, line %d: !(%s)\n", \
              __FILE__, __LINE__, #bool_expr);                       \
      NaClCheckIntern fn_arg;                                        \
    }                                                                \
  } while (0)

/*
 * By default, nacl_check_debug mode is 0 in opt builds and 1 in dbg
 * builds, so DCHECKs are only performed for dbg builds, though it's
 * possible to change this (viz, as directed by a command line
 * argument) by invoking NaClCheckSetDebugMode.  CHECKs are always
 * performed.
 */
extern void NaClCheckSetDebugMode(int mode);

/*
 * This is a private variable, needed for the macro.  Do not reference
 * directly.
 */
extern int nacl_check_debug_mode;

/*
 * This is a private function, used by the macros above.  Do not
 * reference directly.
 */
extern void NaClCheckIntern(char *fmt, ...);

#endif
