/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/utility.h"
#include <stdarg.h>
#include <stdlib.h>
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/include/nacl_assert.h"

namespace ppapi_proxy {

const int foo = 5;

void DebugPrintf(const char* format, ...) {
  static bool printf_enabled = (getenv("NACL_PPAPI_PROXY_DEBUG") != NULL);
  if (printf_enabled) {
    va_list argptr;
    va_start(argptr, format);
#ifdef __native_client__
    fprintf(stdout, "PPAPI_PROXY_PLUGIN : ");
#else
    fprintf(stdout, "PPAPI_PROXY_BROWSER: ");
#endif
    vfprintf(stdout, format, argptr);
    va_end(argptr);
    fflush(stdout);
  }
}

}  // namespace ppapi_proxy
