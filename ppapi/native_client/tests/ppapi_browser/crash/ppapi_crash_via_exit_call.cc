// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

namespace {

// This will crash a PPP_Messaging function.
void CrashViaExitCall() {
  printf("--- CrashViaExitCall\n");
  _exit(0);
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashViaExitCall", CrashViaExitCall);
}

void SetupPluginInterfaces() {
  // none
}
