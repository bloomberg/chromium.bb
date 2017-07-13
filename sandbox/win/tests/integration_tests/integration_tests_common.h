// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_
#define SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_

#include <windows.h>

namespace sandbox {

//------------------------------------------------------------------------------
// Common - for sharing between source files.
//------------------------------------------------------------------------------

enum TestPolicy {
  TESTPOLICY_DEP = 1,
  TESTPOLICY_ASLR,
  TESTPOLICY_STRICTHANDLE,
  TESTPOLICY_WIN32K,
  TESTPOLICY_EXTENSIONPOINT,
  TESTPOLICY_DYNAMICCODE,
  TESTPOLICY_NONSYSFONT,
  TESTPOLICY_MSSIGNED,
  TESTPOLICY_LOADNOREMOTE,
  TESTPOLICY_LOADNOLOW,
  TESTPOLICY_DYNAMICCODEOPTOUT,
  TESTPOLICY_LOADPREFERSYS32
};

// Timeout for ::WaitForSingleObject synchronization.
DWORD SboxTestEventTimeout();

}  // namespace sandbox

#endif  // SANDBOX_TESTS_INTEGRATION_TESTS_COMMON_H_
