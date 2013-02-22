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

typedef std::vector<pp::DirectoryEntry_Dev> Entries;

std::string IntegerToString(int value) {
  char result[12];
  sprintf(result, "%d", value);
  return result;
}

}  // namespace

bool TestDirectoryReader::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestDirectoryReader::RunTests(const std::string& filter) {
  RUN_TEST(ReadEntries, filter);
}

int32_t TestDirectoryReader::DeleteDirectoryRecursively(pp::FileRef* dir) {
  if (!dir)
    return PP_ERROR_BADARGUMENT;

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  TestCompletionCallbackWithOutput<Entries> output_callback(
      instance_->pp_instance(), force_async_);

  int32_t rv = PP_OK;
  pp::DirectoryReader_Dev directory_reader(*dir);
  rv = directory_reader.ReadEntries(output_callback);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = output_callback.WaitForResult();
  if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
    return rv;

  Entries entries = output_callback.output();
  for (Entries::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    pp::FileRef file_ref = it->file_ref();
    if (it->file_type() == PP_FILETYPE_DIRECTORY) {
      rv = DeleteDirectoryRecursively(&file_ref);
      if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
        return rv;
    } else {
      rv = file_ref.Delete(callback);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
        return rv;
    }
  }
  rv = dir->Delete(callback);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  return rv;
}

std::string TestDirectoryReader::TestReadEntries() {
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
  rv = DeleteDirectoryRecursively(&test_dir);
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

  // Test that |ReadEntries()| is able to fetch all directories and files that
  // we created.
  {
    TestCompletionCallbackWithOutput<Entries> output_callback(
        instance_->pp_instance(), force_async_);

    pp::DirectoryReader_Dev directory_reader(test_dir);
    rv = directory_reader.ReadEntries(output_callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("DirectoryReader::ReadEntries force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = output_callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("DirectoryReader::ReadEntries", rv);

    Entries entries = output_callback.output();
    size_t sum = expected_file_names.size() + expected_dir_names.size();
    if (entries.size() != sum)
      return "Expected " + IntegerToString(sum) + " entries, got " +
          IntegerToString(entries.size());

    for (Entries::const_iterator it = entries.begin();
         it != entries.end(); ++it) {
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

  // Test cancellation of asynchronous |ReadEntries()|.
  {
    TestCompletionCallbackWithOutput<Entries> output_callback(
        instance_->pp_instance(), force_async_);

    // Note that the directory reader will be deleted immediately.
    rv = pp::DirectoryReader_Dev(test_dir).ReadEntries(output_callback);
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("DirectoryReader::ReadEntries force_async", rv);
    if (output_callback.run_count() > 0)
      return "DirectoryReader::ReadEntries ran callback synchronously.";

    // If |ReadEntries()| is completing asynchronously, the callback should be
    // aborted (i.e., called with |PP_ERROR_ABORTED| from the message loop)
    // since the resource was destroyed.
    if (rv == PP_OK_COMPLETIONPENDING) {
      rv = output_callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "DirectoryReader::ReadEntries not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("DirectoryReader::ReadEntries", rv);
    }
  }

  PASS();
}
