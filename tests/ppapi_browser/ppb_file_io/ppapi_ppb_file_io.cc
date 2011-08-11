// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_browser/ppb_file_io/common.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

// Open tests
extern void TestOpenExistingFileLocalPersistent();
extern void TestOpenExistingFileLocalTemporary();
extern void TestOpenNonExistingFileLocalPersistent();
extern void TestOpenNonExistingFileLocalTemporary();
// Query tests
extern void TestQueryFileLocalPersistent();
extern void TestQueryFileLocalTemporary();
// Read tests
extern void TestCompleteReadLocalPersistent();
extern void TestCompleteReadLocalTemporary();
extern void TestParallelReadLocalPersistent();
extern void TestParallelReadLocalTemporary();
extern void TestPartialFileReadLocalPersistent();
extern void TestPartialFileReadLocalTemporary();

void SetupTests() {
  // Open tests
  RegisterTest("TestOpenExistingFileLocalPersistent",
               TestOpenExistingFileLocalPersistent);
  RegisterTest("TestOpenExistingFileLocalTemporary",
               TestOpenExistingFileLocalTemporary);
  RegisterTest("TestOpenNonExistingFileLocalPersistent",
               TestOpenNonExistingFileLocalPersistent);
  RegisterTest("TestOpenNonExistingFileLocalTemporary",
               TestOpenNonExistingFileLocalTemporary);
  // Query tests
  RegisterTest("TestQueryFileLocalPersistent",
               TestQueryFileLocalPersistent);
  RegisterTest("TestQueryFileLocalTemporary",
               TestQueryFileLocalTemporary);
  // Rest tests
  RegisterTest("TestCompleteReadLocalPersistent",
               TestCompleteReadLocalPersistent);
  RegisterTest("TestCompleteReadLocalTemporary",
               TestCompleteReadLocalTemporary);
  RegisterTest("TestParallelReadLocalPersistent",
               TestParallelReadLocalPersistent);
  RegisterTest("TestParallelReadLocalTemporary",
               TestParallelReadLocalTemporary);
  RegisterTest("TestPartialFileReadLocalPersistent",
               TestPartialFileReadLocalPersistent);
  RegisterTest("TestPartialFileReadLocalTemporary",
               TestPartialFileReadLocalTemporary);
}

void SetupPluginInterfaces() {
  /* No PPP interface to test. */
}
