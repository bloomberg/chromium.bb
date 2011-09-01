// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/native_client/tests/ppapi_browser/ppb_file_io/common.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"

// Open tests
extern void TestOpenExistingFileLocalTemporary();
extern void TestOpenNonExistingFileLocalTemporary();
// Query tests
extern void TestQueryFileLocalTemporary();
// Read tests
extern void TestCompleteReadLocalTemporary();
extern void TestParallelReadLocalTemporary();
extern void TestPartialFileReadLocalTemporary();

void SetupTests() {
  // Open tests
  RegisterTest("TestOpenExistingFileLocalTemporary",
               TestOpenExistingFileLocalTemporary);
  RegisterTest("TestOpenNonExistingFileLocalTemporary",
               TestOpenNonExistingFileLocalTemporary);
  // Query tests
  RegisterTest("TestQueryFileLocalTemporary",
               TestQueryFileLocalTemporary);
  // Read tests
  RegisterTest("TestCompleteReadLocalTemporary",
               TestCompleteReadLocalTemporary);
  RegisterTest("TestParallelReadLocalTemporary",
               TestParallelReadLocalTemporary);
  RegisterTest("TestPartialFileReadLocalTemporary",
               TestPartialFileReadLocalTemporary);
}

void SetupPluginInterfaces() {
  /* No PPP interface to test. */
}
