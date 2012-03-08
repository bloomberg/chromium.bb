/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Untrusted crash dumper.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_CRASH_DUMP_UNTRUSTED_CRASH_DUMP_H__
#define NATIVE_CLIENT_SRC_UNTRUSTED_CRASH_DUMP_UNTRUSTED_CRASH_DUMP_H__ 1

#include "native_client/src/include/nacl_base.h"


EXTERN_C_BEGIN

/* These return non-zero on success. */
int NaClCrashDumpInit(void);
int NaClCrashDumpInitThread(void);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_CRASH_DUMP_UNTRUSTED_CRASH_DUMP_H__ */
