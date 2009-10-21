/*
* Copyright 2009 The Native Client Authors.  All rights reserved.
* Use of this source code is governed by a BSD-style license that can
* be found in the LICENSE file.
*/

// C/C++ library for handle lookup. Used by IMC in both the renderer and
// sel_ldr process.

#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/handle_pass/ldr_handle.h"

typedef enum {
  HANDLE_PASS_BROKER_PROCESS = 0,  // usually the renderer
  HANDLE_PASS_CLIENT_PROCESS,  // usually the sel_ldr process
} HANDLE_PASS_MODE;

// This function should be called as part of handle passing initialization.
void NaClHandlePassSetLookupMode(HANDLE_PASS_MODE mode);

// This function is called from IMC code and works both in the renderer
// and in the sel_ldr process.
HANDLE NaClHandlePassLookupHandle(DWORD pid);

