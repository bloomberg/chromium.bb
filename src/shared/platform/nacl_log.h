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

enum NaClLogOptions {
  NACL_LOG_OPTIONS_NONE,
  NACL_LOG_OPTIONS_DEFAULT_FROM_ENVIRONMENT
};

/*
 * TODO: per-module logging, adding a module-name parameter, probably
 * an atom that is statically allocated.
 */
void NaClLogModuleInit(void);

void NaClLogModuleInitExtended(enum NaClLogOptions);

/*
 * Sets the log file to the named file.  Aborts program if the open
 * fails.
 *
 * The GioFile object is dynamically allocated, so caller is responsible
 * for obtaining it via NaClLogGetGio and freeing it as appropriate.
 */
void NaClLogSetFile(char const *log_file);

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

/*
 * Users of NaClLogV should add ATTRIBUTE_FORMAT_PRINTF(m,n) to their
 * function prototype, where m is the argument position of the format
 * string and n is the position of the first argument to be consumed
 * by the format specifier.
 */
void NaClLogV(int         detail_level,
              char const  *fmt,
              va_list     ap);

void  NaClLog(int         detail_level,
              char const  *fmt,
              ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);

#define LOG_INFO    (-1)
#define LOG_WARNING (-2)
#define LOG_ERROR   (-3)
#define LOG_FATAL   (-4)

/*
 * Low-level logging code that requires manual lock manipulation.
 * Should be used carefully!
 */
void NaClLogLock(void);
void NaClLogUnlock(void);

/*
 * Caller is responsible for grabbing the NaClLog mutex lock (via
 * NaClLogLock) before calling NaClLogV_mu or NaClLog_mu and for
 * releasing it (via NaClLogUnlock) afterward.
 *
 * Users of NaClLogV_mu should also use ATTRIBUTE_FORMAT_PRINTF as
 * above.
 *
 * Only the first call to NaClLog_mu or NaClLogV_mu after the
 * NaClLogLock will get a timestamp tag prepended.  No newline
 * detection is done, so a multi-line output must be (1) split into
 * multiple calls to NaClLog_mu or NaClLogV_mu so that newlines are
 * the last character in the format string to the NaClLog_mu or
 * NaClLogV_mu calls, and (2) are followed by NaClLogTagNext_mu() so
 * that the next output will know to generate a tag.
 */
void NaClLogV_mu(int         detail_level,
                 char const  *fmt,
                 va_list     ap);

void NaClLog_mu(int         detail_level,
                char const  *fmt,
                ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);

void NaClLogTagNext_mu(void);


EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_LOG_H__ */
