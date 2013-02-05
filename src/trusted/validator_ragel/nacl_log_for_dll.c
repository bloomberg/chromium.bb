/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * We don't want to link whole NaCl platform with validator dynamic library,
 * but we need NaClLog (it is used in CHECK), so we provide our own
 * minimal implementation suitable for testing.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/shared/platform/nacl_log.h"


void NaClLog(int detail_level, char const *fmt, ...) {
  va_list ap;
  UNREFERENCED_PARAMETER(detail_level);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  if (detail_level == LOG_FATAL)
    exit(1);
}
