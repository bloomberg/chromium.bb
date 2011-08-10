/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/x86/error_reporter.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_log.h"

void NaClVerboseErrorPrintf(NaClErrorReporter* self,
                            const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  gvprintf(NaClLogGetGio(), format, ap);
  va_end(ap);
}

void NaClVerboseErrorPrintfV(NaClErrorReporter* self,
                             const char* format,
                             va_list ap) {
  gvprintf(NaClLogGetGio(), format, ap);
}
