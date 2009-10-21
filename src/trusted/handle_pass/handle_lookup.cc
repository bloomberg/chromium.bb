/*
* Copyright 2009 The Native Client Authors.  All rights reserved.
* Use of this source code is governed by a BSD-style license that can
* be found in the LICENSE file.
*/

// C/C++ library for handle lookup. Used by IMC in both the renderer and
// sel_ldr process.

#include "native_client/src/trusted/handle_pass/handle_lookup.h"

#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/handle_pass/ldr_handle.h"

static HANDLE_PASS_MODE handle_pass_mode = HANDLE_PASS_BROKER_PROCESS;

void NaClHandlePassSetLookupMode(HANDLE_PASS_MODE mode) {
  handle_pass_mode = mode;
}

HANDLE NaClHandlePassLookupHandle(DWORD pid) {
  switch (handle_pass_mode) {
    case HANDLE_PASS_BROKER_PROCESS:
      return NaClHandlePassBrowserLookupHandle(pid);
    case HANDLE_PASS_CLIENT_PROCESS:
      return NaClHandlePassLdrLookupHandle(pid);
    default:
      // IMC compares the result to NULL
      return NULL;
  }
}
