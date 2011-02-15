// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_directory_reader.h"

#include <stdio.h>
#include <set>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/directory_reader_dev.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(DirectoryReader);

namespace {

std::string IntegerToString(int value) {
  char result[12];
  sprintf(result, "%d", value);
  return result;
}

}  // namespace

bool TestDirectoryReader::Init() {
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestDirectoryReader::RunTest() {
  RUN_TEST(GetNextFile);
}

std::string TestDirectoryReader::TestGetNextFile() {
  TestCompletionCallback callback(instance_->pp_instance());
  pp::FileSystem_Dev file_system(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileRef_Dev dir_ref(file_system, "/");
  pp::FileRef_Dev file_ref_1(file_system, "/file_1");
  pp::FileRef_Dev file_ref_2(file_system, "/file_2");
  pp::FileRef_Dev file_ref_3(file_system, "/file_3");

  pp::FileIO_Dev file_io_1;
  rv = file_io_1.Open(file_ref_1, PP_FILEOPENFLAG_CREATE, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);
  pp::FileIO_Dev file_io_2;
  rv = file_io_2.Open(file_ref_2, PP_FILEOPENFLAG_CREATE, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);
  pp::FileIO_Dev file_io_3;
  rv = file_io_3.Open(file_ref_3, PP_FILEOPENFLAG_CREATE, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  pp::FileRef_Dev dir_ref_1(file_system, "/dir_1");
  pp::FileRef_Dev dir_ref_2(file_system, "/dir_2");
  pp::FileRef_Dev dir_ref_3(file_system, "/dir_3");
  rv = dir_ref_1.MakeDirectory(callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);
  rv = dir_ref_2.MakeDirectory(callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);
  rv = dir_ref_3.MakeDirectory(callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);

  {
    pp::DirectoryReader_Dev directory_reader(dir_ref);
    std::vector<pp::DirectoryEntry_Dev> entries;
    pp::DirectoryEntry_Dev entry;
    do {
      rv = directory_reader.GetNextEntry(&entry, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("DirectoryReader::GetNextEntry", rv);
      if (!entry.is_null())
        entries.push_back(entry);
    } while (!entry.is_null());

    if (entries.size() != 6)
      return "Expected 6 entries, got " + IntegerToString(entries.size());

    std::set<std::string> expected_file_names;
    expected_file_names.insert("/file_1");
    expected_file_names.insert("/file_2");
    expected_file_names.insert("/file_3");

    std::set<std::string> expected_dir_names;
    expected_dir_names.insert("/dir_1");
    expected_dir_names.insert("/dir_2");
    expected_dir_names.insert("/dir_3");

    for (std::vector<pp::DirectoryEntry_Dev>::const_iterator it =
             entries.begin(); it != entries.end(); it++) {
      pp::FileRef_Dev file_ref = it->file_ref();
      std::string file_path = file_ref.GetPath().AsString();
      std::set<std::string>::iterator found =
          expected_file_names.find(file_path);
      if (found != expected_file_names.end()) {
        if (it->file_type() != PP_FILETYPE_REGULAR)
          return file_path + " should have been a regular file.";
        expected_file_names.erase(found);
      } else {
        found = expected_dir_names.find(file_path);
        if (found == expected_dir_names.end())
          return "Unexpected file path: " + file_path;
        if (it->file_type() != PP_FILETYPE_DIRECTORY)
          return file_path + " should have been a directory.";
        expected_dir_names.erase(found);
      }
    }
    if (!expected_file_names.empty() || !expected_dir_names.empty())
      return "Expected more file paths.";
  }

  // Test cancellation of asynchronous |GetNextEntry()|.
  {
    // Declaring |entry| here prevents memory corruption for some failure modes
    // and lets us to detect some other failures.
    pp::DirectoryEntry_Dev entry;
    callback.reset_run_count();
    // Note that the directory reader will be deleted immediately.
    rv = pp::DirectoryReader_Dev(dir_ref).GetNextEntry(&entry, callback);
    if (callback.run_count() > 0)
      return "DirectoryReader::GetNextEntry ran callback synchronously.";

    // If |GetNextEntry()| is completing asynchronously, the callback should be
    // aborted (i.e., called with |PP_ERROR_ABORTED| from the message loop)
    // since the resource was destroyed.
    if (rv == PP_ERROR_WOULDBLOCK) {
      // |GetNextEntry()| *may* have written to |entry| (e.g., synchronously,
      // before the resource was destroyed), but it must not write to it after
      // destruction.
      bool entry_is_null = entry.is_null();
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "DirectoryReader::GetNextEntry not aborted.";
      if (entry.is_null() != entry_is_null)
        return "DirectoryReader::GetNextEntry wrote result after destruction.";
    } else if (rv != PP_OK) {
      return ReportError("DirectoryReader::GetNextEntry", rv);
    }
  }

  return "";
}
