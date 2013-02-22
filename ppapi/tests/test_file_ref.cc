// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_ref.h"

#include <stdio.h>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/directory_reader_dev.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FileRef);

namespace {

const char* kPersFileName = "persistent";
const char* kTempFileName = "temporary";
const char* kParentPath = "/foo/bar";
const char* kPersFilePath = "/foo/bar/persistent";
const char* kTempFilePath = "/foo/bar/temporary";
#ifndef PPAPI_OS_NACL  // Only used for a test that NaCl can't run yet.
const char* kTerribleName = "!@#$%^&*()-_=+{}[] ;:'\"|`~\t\n\r\b?";
#endif

std::string ReportMismatch(const std::string& method_name,
                           const std::string& returned_result,
                           const std::string& expected_result) {
  return method_name + " returned '" + returned_result + "'; '" +
      expected_result + "' expected.";
}

}  // namespace

bool TestFileRef::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileRef::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Create, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(GetFileSystemType, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(GetName, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(GetPath, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(GetParent, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(MakeDirectory, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(QueryAndTouchFile, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(DeleteFileAndDirectory, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(RenameFileAndDirectory, filter);
#ifndef PPAPI_OS_NACL  // NaCl can't run this test yet.
  RUN_TEST_FORCEASYNC_AND_NOT(FileNameEscaping, filter);
#endif
}

std::string TestFileRef::TestCreate() {
  std::vector<std::string> invalid_paths;
  invalid_paths.push_back("invalid_path");  // no '/' at the first character
  invalid_paths.push_back("");  // empty path
  // The following are directory traversal checks
  invalid_paths.push_back("..");
  invalid_paths.push_back("/../invalid_path");
  invalid_paths.push_back("/../../invalid_path");
  invalid_paths.push_back("/invalid/../../path");
  const size_t num_invalid_paths = invalid_paths.size();

  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  for (size_t j = 0; j < num_invalid_paths; ++j) {
    pp::FileRef file_ref_pers(file_system_pers, invalid_paths[j].c_str());
    if (file_ref_pers.pp_resource() != 0) {
      return "file_ref_pers expected to be invalid for path: " +
          invalid_paths[j];
    }
    pp::FileRef file_ref_temp(file_system_temp, invalid_paths[j].c_str());
    if (file_ref_temp.pp_resource() != 0) {
      return "file_ref_temp expected to be invalid for path: " +
          invalid_paths[j];
    }
  }
  PASS();
}

std::string TestFileRef::TestGetFileSystemType() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  if (file_ref_pers.GetFileSystemType() != PP_FILESYSTEMTYPE_LOCALPERSISTENT)
    return "file_ref_pers expected to be persistent.";

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  if (file_ref_temp.GetFileSystemType() != PP_FILESYSTEMTYPE_LOCALTEMPORARY)
    return "file_ref_temp expected to be temporary.";

  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return "URLLoader::Open() failed.";

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef file_ref_ext(response_info.GetBodyAsFileRef());
  if (file_ref_ext.GetFileSystemType() != PP_FILESYSTEMTYPE_EXTERNAL)
    return "file_ref_ext expected to be external.";

  PASS();
}

std::string TestFileRef::TestGetName() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  std::string name = file_ref_pers.GetName().AsString();
  if (name != kPersFileName)
    return ReportMismatch("FileRef::GetName", name, kPersFileName);

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  name = file_ref_temp.GetName().AsString();
  if (name != kTempFileName)
    return ReportMismatch("FileRef::GetName", name, kTempFileName);

  // Test the "/" case.
  pp::FileRef file_ref_slash(file_system_temp, "/");
  name = file_ref_slash.GetName().AsString();
  if (name != "/")
    return ReportMismatch("FileRef::GetName", name, "/");

  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return "URLLoader::Open() failed.";

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef file_ref_ext(response_info.GetBodyAsFileRef());
  name = file_ref_ext.GetName().AsString();
  if (name == "")
    return ReportMismatch("FileRef::GetName", name, "<a temp file>");

  PASS();
}

std::string TestFileRef::TestGetPath() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  std::string path = file_ref_pers.GetPath().AsString();
  if (path != kPersFilePath)
    return ReportMismatch("FileRef::GetPath", path, kPersFilePath);

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  path = file_ref_temp.GetPath().AsString();
  if (path != kTempFilePath)
    return ReportMismatch("FileRef::GetPath", path, kTempFilePath);

  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return "URLLoader::Open() failed.";

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef file_ref_ext(response_info.GetBodyAsFileRef());
  if (!file_ref_ext.GetPath().is_undefined())
    return "The path of an external FileRef should be void.";

  PASS();
}

std::string TestFileRef::TestGetParent() {
  pp::FileSystem file_system_pers(
      instance_, PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  pp::FileSystem file_system_temp(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);

  pp::FileRef file_ref_pers(file_system_pers, kPersFilePath);
  std::string parent_path = file_ref_pers.GetParent().GetPath().AsString();
  if (parent_path != kParentPath)
    return ReportMismatch("FileRef::GetParent", parent_path, kParentPath);

  pp::FileRef file_ref_temp(file_system_temp, kTempFilePath);
  parent_path = file_ref_temp.GetParent().GetPath().AsString();
  if (parent_path != kParentPath)
    return ReportMismatch("FileRef::GetParent", parent_path, kParentPath);

  // Test the "/" case.
  pp::FileRef file_ref_slash(file_system_temp, "/");
  parent_path = file_ref_slash.GetParent().GetPath().AsString();
  if (parent_path != "/")
    return ReportMismatch("FileRef::GetParent", parent_path, "/");

  // Test the "/foo" case (the parent is "/").
  pp::FileRef file_ref_with_root_parent(file_system_temp, "/foo");
  parent_path = file_ref_with_root_parent.GetParent().GetPath().AsString();
  if (parent_path != "/")
    return ReportMismatch("FileRef::GetParent", parent_path, "/");

  pp::URLRequestInfo request(instance_);
  request.SetURL("test_url_loader_data/hello.txt");
  request.SetStreamToFile(true);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::URLLoader loader(instance_);
  int32_t rv = loader.Open(request, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("URLLoader::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return "URLLoader::Open() failed.";

  pp::URLResponseInfo response_info(loader.GetResponseInfo());
  if (response_info.is_null())
    return "URLLoader::GetResponseInfo returned null";
  int32_t status_code = response_info.GetStatusCode();
  if (status_code != 200)
    return "Unexpected HTTP status code";

  pp::FileRef file_ref_ext(response_info.GetBodyAsFileRef());
  if (!file_ref_ext.GetParent().is_null())
    return "The parent of an external FileRef should be null.";

  PASS();
}

std::string TestFileRef::TestMakeDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  // Open.
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // MakeDirectory.
  pp::FileRef dir_ref(file_system, "/test_dir_make_directory");
  rv = dir_ref.MakeDirectory(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::MakeDirectory", rv);

  // MakeDirectory aborted.
  callback.reset_run_count();
  rv = pp::FileRef(file_system, "/test_dir_make_abort")
      .MakeDirectory(callback);
  if (callback.run_count() > 0)
    return "FileSystem::MakeDirectory ran callback synchronously.";
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileSystem::MakeDirectory not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileSystem::MakeDirectory", rv);
  }

  // MakeDirectoryIncludingAncestors.
  dir_ref = pp::FileRef(file_system, "/dir_make_dir_1/dir_make_dir_2");
  rv = dir_ref.MakeDirectoryIncludingAncestors(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::MakeDirectoryIncludingAncestors", rv);

  // MakeDirectoryIncludingAncestors aborted.
  callback.reset_run_count();
  rv = pp::FileRef(file_system, "/dir_make_abort_1/dir_make_abort_2")
      .MakeDirectoryIncludingAncestors(callback);
  if (callback.run_count() > 0) {
    return "FileSystem::MakeDirectoryIncludingAncestors "
           "ran callback synchronously.";
  }
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError(
        "FileSystem::MakeDirectoryIncludingAncestors force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileSystem::MakeDirectoryIncludingAncestors not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileSystem::MakeDirectoryIncludingAncestors", rv);
  }

  // MakeDirectory with nested path.
  dir_ref = pp::FileRef(file_system, "/dir_make_dir_3/dir_make_dir_4");
  rv = dir_ref.MakeDirectory(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv == PP_OK) {
    return "Calling FileSystem::MakeDirectory() with a nested directory path "
           "should have failed.";
  }

  PASS();
}

std::string TestFileRef::TestQueryAndTouchFile() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileRef file_ref(file_system, "/file_touch");
  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_WRITE,
                    callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Write some data to have a non-zero file size.
  rv = file_io.Write(0, "test", 4, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Write force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != 4)
    return ReportError("FileIO::Write", rv);

  // Touch.
  // last_access_time's granularity is 1 day
  // last_modified_time's granularity is 2 seconds
  const PP_Time last_access_time = 123 * 24 * 3600.0;
  const PP_Time last_modified_time = 246.0;
  rv = file_ref.Touch(last_access_time, last_modified_time, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Touch force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Touch", rv);

  // Touch aborted.
  callback.reset_run_count();
  rv = pp::FileRef(file_system, "/file_touch_abort")
      .Touch(last_access_time, last_modified_time, callback);
  if (callback.run_count() > 0)
    return "FileSystem::Touch ran callback synchronously.";
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Touch force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileSystem::Touch not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileSystem::Touch", rv);
  }

  // Query.
  PP_FileInfo info;
  rv = file_io.Query(&info, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Query force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Query", rv);

  if ((info.size != 4) ||
      (info.type != PP_FILETYPE_REGULAR) ||
      (info.system_type != PP_FILESYSTEMTYPE_LOCALTEMPORARY) ||
      (info.last_access_time != last_access_time) ||
      (info.last_modified_time != last_modified_time))
    return "FileSystem::Query() has returned bad data.";

  // Cancellation test.
  // TODO(viettrungluu): this test causes a bunch of LOG(WARNING)s; investigate.
  callback.reset_run_count();
  // TODO(viettrungluu): check |info| for late writes.
  rv = pp::FileRef(file_system, "/file_touch").Touch(
      last_access_time, last_modified_time, callback);
  if (callback.run_count() > 0)
    return "FileSystem::Touch ran callback synchronously.";
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Touch force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileSystem::Touch not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileSystem::Touch", rv);
  }

  PASS();
}

std::string TestFileRef::TestDeleteFileAndDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileRef file_ref(file_system, "/file_delete");
  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  rv = file_ref.Delete(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Delete force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::Delete", rv);

  pp::FileRef dir_ref(file_system, "/dir_delete");
  rv = dir_ref.MakeDirectory(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);

  rv = dir_ref.Delete(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::Delete", rv);

  pp::FileRef nested_dir_ref(file_system, "/dir_delete_1/dir_delete_2");
  rv = nested_dir_ref.MakeDirectoryIncludingAncestors(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectoryIncludingAncestors", rv);

  // Hang on to a ref to the parent; otherwise the callback will be aborted.
  pp::FileRef parent_dir_ref = nested_dir_ref.GetParent();
  rv = parent_dir_ref.Delete(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_ERROR_FAILED)
    return ReportError("FileRef::Delete", rv);

  pp::FileRef nonexistent_file_ref(file_system, "/nonexistent_file_delete");
  rv = nonexistent_file_ref.Delete(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_ERROR_FILENOTFOUND)
    return ReportError("FileRef::Delete", rv);

  // Delete aborted.
  {
    pp::FileRef file_ref_abort(file_system, "/file_delete_abort");
    pp::FileIO file_io_abort(instance_);
    rv = file_io_abort.Open(file_ref_abort, PP_FILEOPENFLAG_CREATE, callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Open force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileIO::Open", rv);

    callback.reset_run_count();
    rv = file_ref_abort.Delete(callback);
  }
  if (callback.run_count() > 0)
    return "FileRef::Delete ran callback synchronously.";
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileRef::Delete not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileRef::Delete", rv);
  }

  PASS();
}

std::string TestFileRef::TestRenameFileAndDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileRef file_ref(file_system, "/file_rename");
  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  pp::FileRef target_file_ref(file_system, "/target_file_rename");
  rv = file_ref.Rename(target_file_ref, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Rename force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::Rename", rv);

  pp::FileRef dir_ref(file_system, "/dir_rename");
  rv = dir_ref.MakeDirectory(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);

  pp::FileRef target_dir_ref(file_system, "/target_dir_rename");
  rv = dir_ref.Rename(target_dir_ref, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Rename force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::Rename", rv);

  pp::FileRef nested_dir_ref(file_system, "/dir_rename_1/dir_rename_2");
  rv = nested_dir_ref.MakeDirectoryIncludingAncestors(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectoryIncludingAncestors", rv);

  pp::FileRef target_nested_dir_ref(file_system, "/dir_rename_1");
  rv = nested_dir_ref.Rename(target_nested_dir_ref, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_ERROR_FAILED)
    return ReportError("FileRef::Rename", rv);

  // Rename aborted.
  // TODO(viettrungluu): Figure out what we want to do if the target file
  // resource is destroyed before completion.
  pp::FileRef target_file_ref_abort(file_system,
                                        "/target_file_rename_abort");
  {
    pp::FileRef file_ref_abort(file_system, "/file_rename_abort");
    pp::FileIO file_io_abort(instance_);
    rv = file_io_abort.Open(file_ref_abort, PP_FILEOPENFLAG_CREATE, callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Open force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileIO::Open", rv);

    callback.reset_run_count();
    rv = file_ref_abort.Rename(target_file_ref_abort, callback);
  }
  if (callback.run_count() > 0)
    return "FileSystem::Rename ran callback synchronously.";
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Rename force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING) {
    rv = callback.WaitForResult();
    if (rv != PP_ERROR_ABORTED)
      return "FileSystem::Rename not aborted.";
  } else if (rv != PP_OK) {
    return ReportError("FileSystem::Rename", rv);
  }

  PASS();
}

#ifndef PPAPI_OS_NACL
std::string TestFileRef::TestFileNameEscaping() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  std::string test_dir_path = "/dir_for_escaping_test";
  // Create a directory in which to test.
  pp::FileRef test_dir_ref(file_system, test_dir_path.c_str());
  rv = test_dir_ref.MakeDirectory(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);

  // Create the file with the terrible name.
  std::string full_file_path = test_dir_path + "/" + kTerribleName;
  pp::FileRef file_ref(file_system, full_file_path.c_str());
  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // DirectoryReader only works out-of-process.
  if (testing_interface_->IsOutOfProcess()) {
    pp::DirectoryReader_Dev directory_reader(test_dir_ref);
    pp::DirectoryEntry_Dev entry;

    rv = directory_reader.GetNextEntry(&entry, callback);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
      return ReportError("DirectoryEntry_Dev::GetNextEntry", rv);
    if (entry.is_null())
      return "Entry was not found.";
    if (entry.file_ref().GetName().AsString() != kTerribleName)
      return "Entry name did not match.";

    rv = directory_reader.GetNextEntry(&entry, callback);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
      return ReportError("DirectoryEntry_Dev::GetNextEntry", rv);
    if (!entry.is_null())
      return "Directory had too many entries.";
  }

  PASS();
}
#endif
