// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_system.h"

#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FileSystem);

bool TestFileSystem::Init() {
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileSystem::RunTest() {
  RUN_TEST(Open);
  RUN_TEST(MultipleOpens);
}

std::string TestFileSystem::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance());

  // Open.
  pp::FileSystem_Dev file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // Open aborted (see the DirectoryReader test for comments).
  callback.reset_run_count();
  rv = pp::FileSystem_Dev(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY)
      .Open(1024, callback);
  if (callback.run_count() > 0)
    return "FileSystem::Open ran callback synchronously.";
  if (rv == PP_ERROR_WOULDBLOCK) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileSystem::Open not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileSystem::Open", rv);
  }

  PASS();
}

std::string TestFileSystem::TestMultipleOpens() {
  // Should not allow multiple opens, no matter the first open has completed or
  // not.
  TestCompletionCallback callback_1(instance_->pp_instance());
  pp::FileSystem_Dev file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv_1 = file_system.Open(1024, callback_1);
  if (callback_1.run_count() > 0)
    return "FileSystem::Open ran callback synchronously.";

  TestCompletionCallback callback_2(instance_->pp_instance());
  int32_t rv_2 = file_system.Open(1024, callback_2);
  if (rv_2 == PP_ERROR_WOULDBLOCK || rv_2 == PP_OK)
    return "FileSystem::Open should not allow multiple opens.";

  if (rv_1 == PP_ERROR_WOULDBLOCK)
    rv_1 = callback_1.WaitForResult();
  if (rv_1 != PP_OK)
    return ReportError("FileSystem::Open", rv_1);

  TestCompletionCallback callback_3(instance_->pp_instance());
  int32_t rv_3 = file_system.Open(1024, callback_3);
  if (rv_3 == PP_ERROR_WOULDBLOCK || rv_3 == PP_OK)
    return "FileSystem::Open should not allow multiple opens.";

  PASS();
}

