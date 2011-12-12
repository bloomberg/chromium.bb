/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include <windows.h>

int NaClLaunchAndDebugItself(char *program, DWORD *exit_code) {
  return DEBUG_EXCEPTION_HANDLER_NOT_SUPPORTED;
}
int NaClDebugLoop(HANDLE process_handle, DWORD *exit_code) {
  return DEBUG_EXCEPTION_HANDLER_NOT_SUPPORTED;
}