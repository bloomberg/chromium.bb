// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_directory_reader.h"

#include <stdio.h>
#include <set>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/directory_reader_dev.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
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

int32_t DeleteDirectoryRecursively(pp::FileRef* dir,
                                   TestCompletionCallback* callback) {
  if (!dir || !callback)
    return PP_ERROR_BADARGUMENT;

  int32_t rv = PP_OK;
  pp::DirectoryReader_Dev directory_reader(*dir);
  std::vector<pp::DirectoryEntry_Dev> entries;
  pp::DirectoryEntry_Dev entry;
  do {
    rv = directory_reader.GetNextEntry(&entry, *callback);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback->WaitForResult();
    if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
      return rv;
    if (!entry.is_null())
      entries.push_back(entry);
  } while (!entry.is_null());

  for (std::vector<pp::DirectoryEntry_Dev>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    pp::FileRef file_ref = it->file_ref();
    if (it->file_type() == PP_FILETYPE_DIRECTORY) {
      rv = DeleteDirectoryRecursively(&file_ref, callback);
      if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
        return rv;
    } else {
      rv = file_ref.Delete(*callback);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback->WaitForResult();
      if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
        return rv;
    }
  }
  rv = dir->Delete(*callback);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback->WaitForResult();
  return rv;
}

}  // namespace

bool TestDirectoryReader::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestDirectoryReader::RunTests(const std::string& filter) {
  RUN_TEST(GetNextFile, filter);
}

std::string TestDirectoryReader::TestGetNextFile() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(
      instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // Setup testing directories and files.
  const char* test_dir_name = "/test_get_next_file";
  const char* file_prefix = "file_";
  const char* dir_prefix = "dir_";

  pp::FileRef test_dir(file_system, test_dir_name);
  rv = DeleteDirectoryRecursively(&test_dir, &callback);
  if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
    return ReportError("DeleteDirectoryRecursively", rv);

  rv = test_dir.MakeDirectory(callback);
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileRef::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileRef::MakeDirectory", rv);

  std::set<std::string> expected_file_names;
  for (int i = 1; i < 4; ++i) {
    char buffer[40];
    sprintf(buffer, "%s/%s%d", test_dir_name, file_prefix, i);
    pp::FileRef file_ref(file_system, buffer);

    pp::FileIO file_io(instance_);
    rv = file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Open force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileIO::Open", rv);

    expected_file_names.insert(buffer);
  }

  std::set<std::string> expected_dir_names;
  for (int i = 1; i < 4; ++i) {
    char buffer[40];
    sprintf(buffer, "%s/%s%d", test_dir_name, dir_prefix, i);
    pp::FileRef file_ref(file_system, buffer);

    rv = file_ref.MakeDirectory(callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileRef::MakeDirectory force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileRef::MakeDirectory", rv);

    expected_dir_names.insert(buffer);
  }

  // Test that |GetNextEntry()| is able to enumerate all directories and files
  // that we created.
  {
    pp::DirectoryReader_Dev directory_reader(test_dir);
    std::vector<pp::DirectoryEntry_Dev> entries;
    pp::DirectoryEntry_Dev entry;
    do {
      rv = directory_reader.GetNextEntry(&entry, callback);
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("DirectoryReader::GetNextEntry force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("DirectoryReader::GetNextEntry", rv);
      if (!entry.is_null())
        entries.push_back(entry);
    } while (!entry.is_null());

    size_t sum = expected_file_names.size() + expected_dir_names.size();
    if (entries.size() != sum)
      return "Expected " + IntegerToString(sum) + " entries, got " +
             IntegerToString(entries.size());

    for (std::vector<pp::DirectoryEntry_Dev>::const_iterator it =
             entries.begin(); it != entries.end(); ++it) {
      pp::FileRef file_ref = it->file_ref();
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
    rv = pp::DirectoryReader_Dev(test_dir).GetNextEntry(&entry, callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("DirectoryReader::GetNextEntry force_async", rv);
    if (callback.run_count() > 0)
      return "DirectoryReader::GetNextEntry ran callback synchronously.";

    // If |GetNextEntry()| is completing asynchronously, the callback should be
    // aborted (i.e., called with |PP_ERROR_ABORTED| from the message loop)
    // since the resource was destroyed.
    if (rv == PP_OK_COMPLETIONPENDING) {
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

  PASS();
}
