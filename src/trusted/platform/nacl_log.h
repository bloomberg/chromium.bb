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
 * NaCl Server Runtime logging code.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_H__
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_H__

#include <stdarg.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/gio.h"

EXTERN_C_BEGIN

/*
 * TODO: per-module logging, adding a module-name parameter, probably
 * an atom that is statically allocated.
 */
void NaClLogModuleInit(void);

void NaClLogModuleFini(void);

void NaClLogSetVerbosity(int verb);

int NaClLogGetVerbosity(void);

void NaClLogIncrVerbosity(void);

void NaClLogSetGio(struct Gio *out_stream);

struct Gio *NaClLogGetGio(void);

/*
 * Timestamps on log entries may be disabled, e.g., to make it easier to
 * write test that compare logging output.
 */

void  NaClLogEnableTimestamp(void);

void  NaClLogDisableTimestamp(void);

void NaClLogV(int         detail_level,
              char const  *fmt,
              va_list     ap);

void  NaClLog(int         detail_level,
              char const  *fmt,
              ...)
#if !NACL_WINDOWS
    __attribute__((format(printf, 2, 3)))
#endif
;

#define LOG_INFO    (-1)
#define LOG_WARNING (-2)
#define LOG_ERROR   (-3)
#define LOG_FATAL   (-4)

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_H__ */
