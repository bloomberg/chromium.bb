/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/utility.h"

#include <stdarg.h>
#include <stdio.h>
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

bool StringIsUtf8(const char* data, uint32_t len) {
  for (uint32_t i = 0; i < len; i++) {
    if ((data[i] & 0x80) == 0) {
      // Single-byte symbol.
      continue;
    } else if ((data[i] & 0xc0) == 0x80) {
      // Invalid first byte.
      DebugPrintf("Invalid first byte %02x\n", data[i]);
      return false;
    }
    // This is a multi-byte symbol.
    DebugPrintf("Multi-byte %02x\n", data[i]);
    // Discard the uppermost bit.  The remaining high-order bits are the
    // unary count of continuation bytes (up to 5 of them).
    char first = data[i] << 1;
    uint32_t continuation_bytes = 0;
    const uint32_t kMaxContinuationBytes = 5;
    while (first & 0x80) {
      if (++i >= len) {
        DebugPrintf("String ended before enough continuation bytes"
                    "were found.\n");
        return false;
      } else if (++continuation_bytes > kMaxContinuationBytes) {
        DebugPrintf("Too many continuation bytes were requested.\n");
        return false;
      } else if ((data[i] & 0xc0) != 0x80) {
        DebugPrintf("Invalid continuation byte.\n");
        return false;
      }
      first <<= 1;
    }
  }
  return true;
}

}  // namespace ppapi_proxy
