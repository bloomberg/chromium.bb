/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/utility.h"
#include <stdarg.h>
#include <stdlib.h>
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_assert.h"

namespace ppapi_proxy {

const int foo = 5;

void DebugPrintf(const char* format, ...) {
  static int printf_enabled = -1;
  if (printf_enabled == -1) {
    printf_enabled = (getenv("PPAPI_BROWSER_DEBUG") != NULL);
  }
  if (printf_enabled == 1) {
    va_list argptr;
    va_start(argptr, format);
    vfprintf(stderr, format, argptr);
    va_end(argptr);
    fflush(stderr);
  }
}

}  // namespace ppapi_proxy
