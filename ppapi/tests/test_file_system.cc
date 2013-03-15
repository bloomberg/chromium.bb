// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_system.h"

#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FileSystem);

bool TestFileSystem::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileSystem::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Open, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(MultipleOpens, filter);
}

std::string TestFileSystem::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  // Open.
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());

  // Open aborted (see the DirectoryReader test for comments).
  int32_t rv = 0;
  {
    pp::FileSystem fs(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
    rv = fs.Open(1024, callback.GetCallback());
  }
  callback.WaitForAbortResult(rv);
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}

std::string TestFileSystem::TestMultipleOpens() {
  // Should not allow multiple opens, regardless of whether or not the first
  // open has completed.
  TestCompletionCallback callback_1(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv_1 = file_system.Open(1024, callback_1.GetCallback());

  TestCompletionCallback callback_2(instance_->pp_instance(), force_async_);
  callback_2.WaitForResult(file_system.Open(1024, callback_2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_2);
  // FileSystem should not allow multiple opens.
  ASSERT_NE(PP_OK, callback_2.result());

  callback_1.WaitForResult(rv_1);
  CHECK_CALLBACK_BEHAVIOR(callback_1);
  ASSERT_EQ(PP_OK, callback_1.result());

  TestCompletionCallback callback_3(instance_->pp_instance(), force_async_);
  callback_3.WaitForResult(file_system.Open(1024, callback_3.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback_3);
  ASSERT_NE(PP_OK, callback_3.result());

  PASS();
}
