// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileSystem::RunTest() {
  RUN_TEST_FORCEASYNC_AND_NOT(Open);
  RUN_TEST_FORCEASYNC_AND_NOT(MultipleOpens);
}

std::string TestFileSystem::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  // Open.
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // Open aborted (see the DirectoryReader test for comments).
  callback.reset_run_count();
  rv = pp::FileSystem(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY)
      .Open(1024, callback);
  if (callback.run_count() > 0)
    return "FileSystem::Open ran callback synchronously.";
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
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
  TestCompletionCallback callback_1(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv_1 = file_system.Open(1024, callback_1);
  if (callback_1.run_count() > 0)
    return "FileSystem::Open1 ran callback synchronously.";
  if (force_async_ && rv_1 != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open1 force_async", rv_1);

  TestCompletionCallback callback_2(instance_->pp_instance(), force_async_);
  int32_t rv_2 = file_system.Open(1024, callback_2);
  if (force_async_ && rv_2 != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open2 force_async", rv_2);
  if (rv_2 == PP_OK_COMPLETIONPENDING)
    rv_2 = callback_2.WaitForResult();
  if (rv_2 == PP_OK)
    return "FileSystem::Open2 should not allow multiple opens.";

  if (rv_1 == PP_OK_COMPLETIONPENDING)
    rv_1 = callback_1.WaitForResult();
  if (rv_1 != PP_OK)
    return ReportError("FileSystem::Open1", rv_1);

  TestCompletionCallback callback_3(instance_->pp_instance(), force_async_);
  int32_t rv_3 = file_system.Open(1024, callback_3);
  if (force_async_ && rv_3 != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open3 force_async", rv_3);
  if (rv_3 == PP_OK_COMPLETIONPENDING)
    rv_3 = callback_3.WaitForResult();
  if (rv_3 == PP_OK)
    return "FileSystem::Open3 should not allow multiple opens.";

  PASS();
}
