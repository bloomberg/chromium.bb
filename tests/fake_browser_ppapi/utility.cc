// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

namespace fake_browser_ppapi {

void DebugPrintf(const char* format, ...) {
  va_list argptr;
  va_start(argptr, format);
  fprintf(stdout, "fake_browser: ");
  vfprintf(stdout, format, argptr);
  va_end(argptr);
  fflush(stdout);
}

}  // namespace fake_browser_ppapi
