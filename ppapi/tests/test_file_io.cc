// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_io.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <vector>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/file_io_private.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

#if defined(PPAPI_OS_WIN)
# include <io.h>
# include <windows.h>
// TODO(hamaji): Use standard windows APIs instead of compatibility layer?
# define lseek _lseek
# define read _read
# define write _write
# define ssize_t int
#else
# include <sys/mman.h>
# include <unistd.h>
#endif

REGISTER_TEST_CASE(FileIO);

namespace {

std::string ReportMismatch(const std::string& method_name,
                           const std::string& returned_result,
                           const std::string& expected_result) {
  return method_name + " returned '" + returned_result + "'; '" +
      expected_result + "' expected.";
}

std::string ReportOpenError(int32_t open_flags) {
  static const char* kFlagNames[] = {
    "PP_FILEOPENFLAG_READ",
    "PP_FILEOPENFLAG_WRITE",
    "PP_FILEOPENFLAG_CREATE",
    "PP_FILEOPENFLAG_TRUNCATE",
    "PP_FILEOPENFLAG_EXCLUSIVE"
  };

  std::string result = "FileIO:Open had unexpected behavior with flags: ";
  bool first_flag = true;
  for (int32_t mask = 1, index = 0; mask <= PP_FILEOPENFLAG_EXCLUSIVE;
       mask <<= 1, ++index) {
    if (mask & open_flags) {
      if (first_flag) {
        first_flag = false;
      } else {
        result += " | ";
      }
      result += kFlagNames[index];
    }
  }
  if (first_flag)
    result += "[None]";

  return result;
}

int32_t ReadEntireFile(PP_Instance instance,
                       pp::FileIO* file_io,
                       int32_t offset,
                       std::string* data) {
  TestCompletionCallback callback(instance);
  char buf[256];
  int32_t read_offset = offset;

  for (;;) {
    int32_t rv = file_io->Read(read_offset, buf, sizeof(buf),
                               callback.GetCallback());
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return rv;
    if (rv == 0)
      break;
    read_offset += rv;
    data->append(buf, rv);
  }

  return PP_OK;
}

int32_t ReadToArrayEntireFile(PP_Instance instance,
                              pp::FileIO* file_io,
                              int32_t offset,
                              std::string* data,
                              bool force_async) {
  TestCompletionCallbackWithOutput< std::vector<char> > callback(
      instance, force_async);

  for (;;) {
    callback.WaitForResult(file_io->Read(offset, 256, callback.GetCallback()));
    int32_t rv = callback.result();
    if (rv < 0)
      return rv;
    if (rv == 0)
      break;
    assert(rv == static_cast<int32_t>(callback.output().size()));
    offset += rv;
    data->append(callback.output().begin(), callback.output().end());
  }

  return PP_OK;
}

int32_t WriteEntireBuffer(PP_Instance instance,
                          pp::FileIO* file_io,
                          int32_t offset,
                          const std::string& data) {
  TestCompletionCallback callback(instance);
  int32_t write_offset = offset;
  const char* buf = data.c_str();
  int32_t size = data.size();

  while (write_offset < offset + size) {
    int32_t rv = file_io->Write(write_offset, &buf[write_offset - offset],
                                size - write_offset + offset,
                                callback.GetCallback());
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv < 0)
      return rv;
    if (rv == 0)
      return PP_ERROR_FAILED;
    write_offset += rv;
  }

  return PP_OK;
}

}  // namespace

bool TestFileIO::Init() {
  return CheckTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileIO::RunTests(const std::string& filter) {
  RUN_TEST_FORCEASYNC_AND_NOT(Open, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(OpenDirectory, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ReadWriteSetLength, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ReadToArrayWriteSetLength, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(TouchQuery, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(AbortCalls, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ParallelReads, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(ParallelWrites, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(NotAllowMixedReadWrite, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(WillWriteWillSetLength, filter);
  RUN_TEST_FORCEASYNC_AND_NOT(RequestOSFileHandle, filter);

  // TODO(viettrungluu): add tests:
  //  - that PP_ERROR_PENDING is correctly returned
  //  - that operations respect the file open modes (flags)
}

std::string TestFileIO::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_open");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  std::string result;
  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_READ,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  // Test the behavior of the power set of
  //   { PP_FILEOPENFLAG_CREATE,
  //     PP_FILEOPENFLAG_TRUNCATE,
  //     PP_FILEOPENFLAG_EXCLUSIVE }.

  // First of all, none of them are specified.
  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
      CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_EXCLUSIVE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_TRUNCATE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE |
      PP_FILEOPENFLAG_EXCLUSIVE,
      CREATE_IF_DOESNT_EXIST | DONT_OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_TRUNCATE,
      CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_EXCLUSIVE |
      PP_FILEOPENFLAG_TRUNCATE,
      DONT_CREATE_IF_DOESNT_EXIST | OPEN_IF_EXISTS | TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE |
      PP_FILEOPENFLAG_EXCLUSIVE | PP_FILEOPENFLAG_TRUNCATE,
      CREATE_IF_DOESNT_EXIST | DONT_OPEN_IF_EXISTS | DONT_TRUNCATE_IF_EXISTS);
  if (!result.empty())
    return result;

  // Invalid combination: PP_FILEOPENFLAG_TRUNCATE without
  // PP_FILEOPENFLAG_WRITE.
  result = MatchOpenExpectations(
      &file_system,
      PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_TRUNCATE,
      INVALID_FLAG_COMBINATION);
  if (!result.empty())
    return result;

  PASS();
}

std::string TestFileIO::TestOpenDirectory() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // Make a directory.
  pp::FileRef dir_ref(file_system, "/test_dir_open_directory");
  rv = dir_ref.MakeDirectory(callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::MakeDirectory force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::MakeDirectory", rv);

  // Open the directory. This is expected to fail since directories cannot be
  // opened.
  pp::FileIO file_io(instance_);
  rv = file_io.Open(dir_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  // Check for failing open operation for the directory.
  if (rv != PP_ERROR_NOTAFILE)
    return ReportError("FileIO::Open", rv);

  PASS();
}

std::string TestFileIO::TestReadWriteSetLength() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_read_write_setlength");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Write something to the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, "test_test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Check for failing read operation.
  char buf[256];
  rv = file_io.Read(0, buf, -1,  // negative number of bytes to read
                    callback.GetCallback());
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_ERROR_FAILED)
    return ReportError("FileIO::Read", rv);

  // Read the entire file.
  std::string read_buffer;
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test_test")
    return ReportMismatch("FileIO::Read", read_buffer, "test_test");

  // Truncate the file.
  rv = file_io.SetLength(4, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::SetLength force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::SetLength", rv);

  // Check the file contents.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test")
    return ReportMismatch("FileIO::Read", read_buffer, "test");

  // Try to read past the end of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 100, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (!read_buffer.empty())
    return ReportMismatch("FileIO::Read", read_buffer, "<empty string>");

  // Write past the end of the file. The file should be zero-padded.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 8, "test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("test\0\0\0\0test", 12))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("test\0\0\0\0test", 12));

  // Extend the file.
  rv = file_io.SetLength(16, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::SetLength force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::SetLength", rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("test\0\0\0\0test\0\0\0\0", 16))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("test\0\0\0\0test\0\0\0\0", 16));

  // Write in the middle of the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 4, "test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("testtesttest\0\0\0\0", 16))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("testtesttest\0\0\0\0", 16));

  // Read from the middle of the file.
  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 4, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("testtest\0\0\0\0", 12))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("testtest\0\0\0\0", 12));

  PASS();
}

// This is basically a copy of TestReadWriteSetLength, but with the new Read
// API.  With this test case, we can make sure the two Read's have the same
// behavior.
std::string TestFileIO::TestReadToArrayWriteSetLength() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_read_write_setlength");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Write something to the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, "test_test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  TestCompletionCallbackWithOutput< std::vector<char> > callback2(
      instance_->pp_instance(), force_async_);
  // Check for failing read operation.
  callback2.WaitForResult(
      file_io.Read(0, -1,  // negative number of bytes to read
                   callback2.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback2);
  if (callback2.result() != PP_ERROR_FAILED)
    return ReportError("FileIO::Read", callback2.result());

  // Read the entire file.
  std::string read_buffer;
  read_buffer.reserve(10);
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test_test")
    return ReportMismatch("FileIO::Read", read_buffer, "test_test");

  // Truncate the file.
  rv = file_io.SetLength(4, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::SetLength force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::SetLength", rv);

  // Check the file contents.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test")
    return ReportMismatch("FileIO::Read", read_buffer, "test");

  // Try to read past the end of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 100,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (!read_buffer.empty())
    return ReportMismatch("FileIO::Read", read_buffer, "<empty string>");

  // Write past the end of the file. The file should be zero-padded.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 8, "test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("test\0\0\0\0test", 12))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("test\0\0\0\0test", 12));

  // Extend the file.
  rv = file_io.SetLength(16, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::SetLength force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::SetLength", rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("test\0\0\0\0test\0\0\0\0", 16))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("test\0\0\0\0test\0\0\0\0", 16));

  // Write in the middle of the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 4, "test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Check the contents of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 0,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("testtesttest\0\0\0\0", 16))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("testtesttest\0\0\0\0", 16));

  // Read from the middle of the file.
  read_buffer.clear();
  rv = ReadToArrayEntireFile(instance_->pp_instance(), &file_io, 4,
                             &read_buffer, force_async_);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != std::string("testtest\0\0\0\0", 12))
    return ReportMismatch("FileIO::Read", read_buffer,
                          std::string("testtest\0\0\0\0", 12));

  PASS();
}

std::string TestFileIO::TestTouchQuery() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback.GetCallback());
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
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Write some data to have a non-zero file size.
  rv = file_io.Write(0, "test", 4, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Write force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != 4)
    return ReportError("FileIO::Write", rv);

  // last_access_time's granularity is 1 day
  // last_modified_time's granularity is 2 seconds
  const PP_Time last_access_time = 123 * 24 * 3600.0;
  const PP_Time last_modified_time = 246.0;
  rv = file_io.Touch(last_access_time, last_modified_time,
                     callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Touch force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Touch", rv);

  PP_FileInfo info;
  rv = file_io.Query(&info, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Query force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Query", rv);

  if ((info.size != 4) ||
      (info.type != PP_FILETYPE_REGULAR) ||
      (info.system_type != PP_FILESYSTEMTYPE_LOCALTEMPORARY) ||
      (info.last_access_time != last_access_time) ||
      (info.last_modified_time != last_modified_time))
    return "FileIO::Query() has returned bad data.";

  // Call |Query()| again, to make sure it works a second time.
  rv = file_io.Query(&info, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Query force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Query", rv);

  PASS();
}

std::string TestFileIO::TestAbortCalls() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_abort_calls");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // First, create a file which to do ops on.
  {
    pp::FileIO file_io(instance_);
    rv = file_io.Open(file_ref,
                      PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
                      callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Open force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileIO::Open", rv);

    // N.B.: Should write at least 3 bytes.
    rv = WriteEntireBuffer(instance_->pp_instance(),
                           &file_io,
                           0,
                           "foobarbazquux");
    if (rv != PP_OK)
      return ReportError("FileIO::Write", rv);
  }

  // Abort |Open()|.
  {
    callback.reset_run_count();
    rv = pp::FileIO(instance_).Open(file_ref, PP_FILEOPENFLAG_READ,
                                    callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Open force_async", rv);
    if (callback.run_count() > 0)
      return "FileIO::Open ran callback synchronously.";
    if (rv == PP_OK_COMPLETIONPENDING) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Open not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Open", rv);
    }
  }

  // Abort |Query()|.
  {
    PP_FileInfo info = { 0 };
    {
      pp::FileIO file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Query(&info, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Query force_async", rv);
    }  // Destroy |file_io|.
    if (rv == PP_OK_COMPLETIONPENDING) {
      // Save a copy and make sure |info| doesn't get written to.
      PP_FileInfo info_copy;
      memcpy(&info_copy, &info, sizeof(info));
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Query not aborted.";
      if (memcmp(&info_copy, &info, sizeof(info)) != 0)
        return "FileIO::Query wrote data after resource destruction.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Query", rv);
    }
  }

  // Abort |Touch()|.
  {
    {
      pp::FileIO file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE,
                        callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Touch(0, 0, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Touch force_async", rv);
    }  // Destroy |file_io|.
    if (rv == PP_OK_COMPLETIONPENDING) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Touch not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Touch", rv);
    }
  }

  // Abort |Read()|.
  {
    char buf[3] = { 0 };
    {
      pp::FileIO file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Read(0, buf, sizeof(buf), callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Read force_async", rv);
    }  // Destroy |file_io|.
    if (rv == PP_OK_COMPLETIONPENDING) {
      // Save a copy and make sure |buf| doesn't get written to.
      char buf_copy[3];
      memcpy(&buf_copy, &buf, sizeof(buf));
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Read not aborted.";
      if (memcmp(&buf_copy, &buf, sizeof(buf)) != 0)
        return "FileIO::Read wrote data after resource destruction.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Read", rv);
    }
  }

  // Abort |Write()|.
  {
    char buf[3] = { 0 };
    {
      pp::FileIO file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Write(0, buf, sizeof(buf), callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Write force_async", rv);
    }  // Destroy |file_io|.
    if (rv == PP_OK_COMPLETIONPENDING) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Write not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Write", rv);
    }
  }

  // Abort |SetLength()|.
  {
    {
      pp::FileIO file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.SetLength(3, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::SetLength force_async", rv);
    }  // Destroy |file_io|.
    if (rv == PP_OK_COMPLETIONPENDING) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::SetLength not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::SetLength", rv);
    }
  }

  // Abort |Flush()|.
  {
    {
      pp::FileIO file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Open force_async", rv);
      if (rv == PP_OK_COMPLETIONPENDING)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Flush(callback.GetCallback());
      if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Flush force_async", rv);
    }  // Destroy |file_io|.
    if (rv == PP_OK_COMPLETIONPENDING) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Flush not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Flush", rv);
    }
  }

  // TODO(viettrungluu): Also test that Close() aborts callbacks.
  // crbug.com/69457

  PASS();
}

std::string TestFileIO::TestParallelReads() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_parallel_reads");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Set up testing contents.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, "abcdefghijkl");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Parallel read operations.
  const char* border = "__border__";
  const int32_t border_size = strlen(border);

  TestCompletionCallback callback_1(instance_->pp_instance(), force_async_);
  int32_t read_offset_1 = 0;
  int32_t size_1 = 3;
  std::vector<char> extended_buf_1(border_size * 2 + size_1);
  char* buf_1 = &extended_buf_1[border_size];
  memcpy(&extended_buf_1[0], border, border_size);
  memcpy(buf_1 + size_1, border, border_size);

  TestCompletionCallback callback_2(instance_->pp_instance(), force_async_);
  int32_t read_offset_2 = size_1;
  int32_t size_2 = 9;
  std::vector<char> extended_buf_2(border_size * 2 + size_2);
  char* buf_2 = &extended_buf_2[border_size];
  memcpy(&extended_buf_2[0], border, border_size);
  memcpy(buf_2 + size_2, border, border_size);

  int32_t rv_1 = PP_OK;
  int32_t rv_2 = PP_OK;
  while (size_1 >= 0 && size_2 >= 0 && size_1 + size_2 > 0) {
    if (size_1 > 0) {
      rv_1 = file_io.Read(read_offset_1, buf_1, size_1,
                          callback_1.GetCallback());
      if (rv_1 != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Read", rv_1);
    }

    if (size_2 > 0) {
      rv_2 = file_io.Read(read_offset_2, buf_2, size_2,
                          callback_2.GetCallback());
      if (rv_2 != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Read", rv_2);
    }

    if (size_1 > 0) {
      rv_1 = callback_1.WaitForResult();
      if (rv_1 <= 0)
        return ReportError("FileIO::Read", rv_1);
      read_offset_1 += rv_1;
      buf_1 += rv_1;
      size_1 -= rv_1;
    }

    if (size_2 > 0) {
      rv_2 = callback_2.WaitForResult();
      if (rv_2 <= 0)
        return ReportError("FileIO::Read", rv_2);
      read_offset_2 += rv_2;
      buf_2 += rv_2;
      size_2 -= rv_2;
    }
  }

  // If |size_1| or |size_2| is less than 0, we have invoked wrong
  // callback(s).
  if (size_1 < 0 || size_2 < 0) {
    return std::string(
        "Parallel FileIO::Read operations have invoked wrong callbacks.");
  }

  // Make sure every read operation writes into the correct buffer.
  const char expected_result_1[] = "__border__abc__border__";
  const char expected_result_2[] = "__border__defghijkl__border__";
  if (strncmp(&extended_buf_1[0], expected_result_1,
              strlen(expected_result_1)) != 0 ||
      strncmp(&extended_buf_2[0], expected_result_2,
              strlen(expected_result_2)) != 0) {
    return std::string(
        "Parallel FileIO::Read operations have written into wrong buffers.");
  }

  PASS();
}

std::string TestFileIO::TestParallelWrites() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_parallel_writes");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Parallel write operations.
  TestCompletionCallback callback_1(instance_->pp_instance(), force_async_);
  int32_t write_offset_1 = 0;
  const char* buf_1 = "abc";
  int32_t size_1 = strlen(buf_1);

  TestCompletionCallback callback_2(instance_->pp_instance(), force_async_);
  int32_t write_offset_2 = size_1;
  const char* buf_2 = "defghijkl";
  int32_t size_2 = strlen(buf_2);

  int32_t rv_1 = PP_OK;
  int32_t rv_2 = PP_OK;
  while (size_1 >= 0 && size_2 >= 0 && size_1 + size_2 > 0) {
    if (size_1 > 0) {
      rv_1 = file_io.Write(write_offset_1, buf_1, size_1,
                           callback_1.GetCallback());
      if (rv_1 != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Write", rv_1);
    }

    if (size_2 > 0) {
      rv_2 = file_io.Write(write_offset_2, buf_2, size_2,
                           callback_2.GetCallback());
      if (rv_2 != PP_OK_COMPLETIONPENDING)
        return ReportError("FileIO::Write", rv_2);
    }

    if (size_1 > 0) {
      rv_1 = callback_1.WaitForResult();
      if (rv_1 <= 0)
        return ReportError("FileIO::Write", rv_1);
      write_offset_1 += rv_1;
      buf_1 += rv_1;
      size_1 -= rv_1;
    }

    if (size_2 > 0) {
      rv_2 = callback_2.WaitForResult();
      if (rv_2 <= 0)
        return ReportError("FileIO::Write", rv_2);
      write_offset_2 += rv_2;
      buf_2 += rv_2;
      size_2 -= rv_2;
    }
  }

  // If |size_1| or |size_2| is less than 0, we have invoked wrong
  // callback(s).
  if (size_1 < 0 || size_2 < 0) {
    return std::string(
        "Parallel FileIO::Write operations have invoked wrong callbacks.");
  }

  // Check the file contents.
  std::string read_buffer;
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "abcdefghijkl")
    return ReportMismatch("FileIO::Read", read_buffer, "abcdefghijkl");

  PASS();
}

std::string TestFileIO::TestNotAllowMixedReadWrite() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_not_allow_mixed_read_write");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Cannot read and write in parallel.
  TestCompletionCallback callback_1(instance_->pp_instance(), force_async_);
  int32_t write_offset_1 = 0;
  const char* buf_1 = "mnopqrstuvw";
  int32_t rv_1 = file_io.Write(write_offset_1, buf_1, strlen(buf_1),
                               callback_1.GetCallback());
  if (rv_1 != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Write", rv_1);

  TestCompletionCallback callback_2(instance_->pp_instance(), force_async_);
  int32_t read_offset_2 = 4;
  char buf_2[3];
  int32_t rv_2 = file_io.Read(read_offset_2, buf_2, sizeof(buf_2),
                              callback_2.GetCallback());
  if (force_async_ && rv_2 != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Read force_async", rv_2);
  if (rv_2 == PP_OK_COMPLETIONPENDING)
    rv_2 = callback_2.WaitForResult();
  if (rv_2 != PP_ERROR_INPROGRESS)
    return ReportError("FileIO::Read", rv_2);
  callback_1.WaitForResult();

  // Cannot query while a write is pending.
  rv_1 = file_io.Write(write_offset_1, buf_1, strlen(buf_1),
                       callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);
  TestCompletionCallback callback_3(instance_->pp_instance(), force_async_);
  PP_FileInfo info;
  int32_t rv_3 = file_io.Query(&info, callback_3.GetCallback());
  if (rv_3 == PP_OK_COMPLETIONPENDING)
    rv_3 = callback_3.WaitForResult();
  if (rv_3 != PP_ERROR_INPROGRESS)
    return ReportError("FileIO::Query", rv_3);
  callback_1.WaitForResult();

  // Cannot touch while a write is pending.
  rv_1 = file_io.Write(write_offset_1, buf_1, strlen(buf_1),
                       callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);
  TestCompletionCallback callback_4(instance_->pp_instance(), force_async_);
  int32_t rv_4 = file_io.Touch(1234.0, 5678.0, callback_4.GetCallback());
  if (rv_4 == PP_OK_COMPLETIONPENDING)
    rv_4 = callback_4.WaitForResult();
  if (rv_4 != PP_ERROR_INPROGRESS)
    return ReportError("FileIO::Touch", rv_4);
  callback_1.WaitForResult();

  // Cannot set length while a write is pending.
  rv_1 = file_io.Write(write_offset_1, buf_1, strlen(buf_1),
                       callback_1.GetCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, rv_1);
  TestCompletionCallback callback_5(instance_->pp_instance(), force_async_);
  int32_t rv_5 = file_io.SetLength(123, callback_5.GetCallback());
  if (rv_5 == PP_OK_COMPLETIONPENDING)
    rv_5 = callback_5.WaitForResult();
  if (rv_5 != PP_ERROR_INPROGRESS)
    return ReportError("FileIO::SetLength", rv_5);
  callback_1.WaitForResult();

  PASS();
}

std::string TestFileIO::TestWillWriteWillSetLength() {
  TestCompletionCallback callback(instance_->pp_instance(), force_async_);

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_will_write");
  int32_t rv = file_system.Open(1024, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileSystem::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_TRUNCATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  const PPB_FileIOTrusted* trusted = static_cast<const PPB_FileIOTrusted*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FILEIOTRUSTED_INTERFACE));
  if (!trusted)
    return ReportError("FileIOTrusted", PP_ERROR_FAILED);

  // Get file descriptor. This is only supported in-process for now, so don't
  // test out of process.
  const PPB_Testing_Dev* testing_interface = GetTestingInterface();
  if (testing_interface && !testing_interface->IsOutOfProcess()) {
    int32_t fd = trusted->GetOSFileDescriptor(file_io.pp_resource());
    if (fd < 0)
      return "FileIO::GetOSFileDescriptor() returned a bad file descriptor.";
  }

  // Calling WillWrite.
  rv = trusted->WillWrite(
      file_io.pp_resource(), 0, 9,
      callback.GetCallback().pp_completion_callback());
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != 9)
    return ReportError("WillWrite", rv);

  // Writing the actual data.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, "test_test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  std::string read_buffer;
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test_test")
    return ReportMismatch("FileIO::Read", read_buffer, "test_test");

  // Calling WillSetLength.
  rv = trusted->WillSetLength(
      file_io.pp_resource(), 4,
      callback.GetCallback().pp_completion_callback());
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("WillSetLength", rv);

  // Calling actual SetLength.
  rv = file_io.SetLength(4, callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::SetLength force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::SetLength", rv);

  read_buffer.clear();
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test")
    return ReportMismatch("FileIO::Read", read_buffer, "test");

  PASS();
}

std::string TestFileIO::TestRequestOSFileHandle() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef file_ref(file_system, "/file_os_fd");

  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  if (callback.result() != PP_OK)
    return ReportError("FileSystem::Open", callback.result());

  pp::FileIO_Private file_io(instance_);
  callback.WaitForResult(file_io.Open(file_ref,
                                      PP_FILEOPENFLAG_CREATE |
                                      PP_FILEOPENFLAG_TRUNCATE |
                                      PP_FILEOPENFLAG_READ |
                                      PP_FILEOPENFLAG_WRITE,
                                      callback.GetCallback()));
  if (callback.result() != PP_OK)
    return ReportError("FileIO::Open", callback.result());

  PP_FileHandle handle = PP_kInvalidFileHandle;
  callback.WaitForResult(
      file_io.RequestOSFileHandle(&handle, callback.GetCallback()));
  if (callback.result() != PP_OK)
    return ReportError("FileIO::RequestOSFileHandle", callback.result());

  if (handle == PP_kInvalidFileHandle)
    return "FileIO::RequestOSFileHandle() returned a bad file handle.";
#if defined(PPAPI_OS_WIN)
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                           _O_RDWR | _O_BINARY);
#else
  int fd = handle;
#endif
  if (fd < 0)
    return "FileIO::RequestOSFileHandle() returned a bad file descriptor.";

  // Check write(2) for the native FD.
  const std::string msg = "foobar";
  ssize_t cnt = write(fd, msg.data(), msg.size());
  if (cnt < 0)
    return ReportError("write for native FD returned error", errno);
  if (cnt != static_cast<ssize_t>(msg.size()))
    return ReportError("write for native FD count mismatch", cnt);

  // Check lseek(2) for the native FD.
  off_t off = lseek(fd, 0, SEEK_CUR);
  if (off == static_cast<off_t>(-1))
    return ReportError("lseek for native FD returned error", errno);
  if (off != static_cast<off_t>(msg.size()))
    return ReportError("lseek for native FD offset mismatch", off);

  off = lseek(fd, 0, SEEK_SET);
  if (off == static_cast<off_t>(-1))
    return ReportError("lseek for native FD returned error", errno);
  if (off != 0)
    return ReportError("lseek for native FD offset mismatch", off);

  // Check read(2) for the native FD.
  std::string buf(msg.size(), '\0');
  cnt = read(fd, &buf[0], msg.size());
  if (cnt < 0)
    return ReportError("read for native FD returned error", errno);
  if (cnt != static_cast<ssize_t>(msg.size()))
    return ReportError("read for native FD count mismatch", cnt);
  if (msg != buf)
    return ReportMismatch("read for native FD", buf, msg);

  // TODO(hamaji): Test CreateFileMapping for windows.
#if !defined(PPAPI_OS_WIN)
  // Check mmap(2) for read.
  char* mapped = reinterpret_cast<char*>(
      mmap(NULL, msg.size(), PROT_READ, MAP_PRIVATE, fd, 0));
  if (mapped == MAP_FAILED)
    return ReportError("mmap(r) for native FD returned errno", errno);
  // Make sure the buffer is cleared.
  buf = std::string(msg.size(), '\0');
  memcpy(&buf[0], mapped, msg.size());
  if (msg != buf)
    return ReportMismatch("mmap(r) for native FD", buf, msg);
  int r = munmap(mapped, msg.size());
  if (r < 0)
    return ReportError("munmap for native FD returned error", errno);

  // Check mmap(2) for write.
  mapped = reinterpret_cast<char*>(
      mmap(NULL, msg.size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
  if (mapped == MAP_FAILED)
    return ReportError("mmap(w) for native FD returned errno", errno);
  // s/foo/baz/
  strcpy(mapped, "baz");

  r = munmap(mapped, msg.size());
  if (r < 0)
    return ReportError("munmap for native FD returned error", errno);
#endif

#if defined(PPAPI_OS_WIN)
  int r = _close(fd);
#else
  r = close(handle);
#endif
  if (r < 0)
    return ReportError("close for native FD returned error", errno);

  // TODO(hamaji): Check if the file is actually updated?

  PASS();
}

std::string TestFileIO::MatchOpenExpectations(pp::FileSystem* file_system,
                                              size_t open_flags,
                                              size_t expectations) {
  std::string bad_argument =
      "TestFileIO::MatchOpenExpectations has invalid input arguments.";
  bool invalid_combination = !!(expectations & INVALID_FLAG_COMBINATION);
  if (invalid_combination) {
    if (expectations != INVALID_FLAG_COMBINATION)
      return bad_argument;
  } else {
    // Validate that one and only one of <some_expectation> and
    // DONT_<some_expectation> is specified.
    for (size_t remains = expectations, end = END_OF_OPEN_EXPECATION_PAIRS;
         end != 0; remains >>= 2, end >>= 2) {
      if (!!(remains & 1) == !!(remains & 2))
        return bad_argument;
    }
  }
  bool create_if_doesnt_exist = !!(expectations & CREATE_IF_DOESNT_EXIST);
  bool open_if_exists = !!(expectations & OPEN_IF_EXISTS);
  bool truncate_if_exists = !!(expectations & TRUNCATE_IF_EXISTS);

  TestCompletionCallback callback(instance_->pp_instance(), force_async_);
  pp::FileRef existent_file_ref(
      *file_system, "/match_open_expectation_existent_non_empty_file");
  pp::FileRef nonexistent_file_ref(
      *file_system, "/match_open_expectation_nonexistent_file");

  // Setup files for test.
  {
    int32_t rv = existent_file_ref.Delete(callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileRef::Delete force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
      return ReportError("FileRef::Delete", rv);

    rv = nonexistent_file_ref.Delete(callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileRef::Delete force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK && rv != PP_ERROR_FILENOTFOUND)
      return ReportError("FileRef::Delete", rv);

    pp::FileIO existent_file_io(instance_);
    rv = existent_file_io.Open(existent_file_ref,
                               PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
                               callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Open force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileIO::Open", rv);

    rv = WriteEntireBuffer(instance_->pp_instance(), &existent_file_io, 0,
                           "foobar");
    if (rv != PP_OK)
      return ReportError("FileIO::Write", rv);
  }

  pp::FileIO existent_file_io(instance_);
  int32_t rv = existent_file_io.Open(existent_file_ref, open_flags,
                                     callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if ((invalid_combination && rv == PP_OK) ||
      (!invalid_combination && ((rv == PP_OK) != open_if_exists))) {
    return ReportOpenError(open_flags);
  }

  if (!invalid_combination && open_if_exists) {
    PP_FileInfo info;
    rv = existent_file_io.Query(&info, callback.GetCallback());
    if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
      return ReportError("FileIO::Query force_async", rv);
    if (rv == PP_OK_COMPLETIONPENDING)
      rv = callback.WaitForResult();
    if (rv != PP_OK)
      return ReportError("FileIO::Query", rv);

    if (truncate_if_exists != (info.size == 0))
      return ReportOpenError(open_flags);
  }

  pp::FileIO nonexistent_file_io(instance_);
  rv = nonexistent_file_io.Open(nonexistent_file_ref, open_flags,
                                callback.GetCallback());
  if (force_async_ && rv != PP_OK_COMPLETIONPENDING)
    return ReportError("FileIO::Open force_async", rv);
  if (rv == PP_OK_COMPLETIONPENDING)
    rv = callback.WaitForResult();
  if ((invalid_combination && rv == PP_OK) ||
      (!invalid_combination && ((rv == PP_OK) != create_if_doesnt_exist))) {
    return ReportOpenError(open_flags);
  }

  return std::string();
}

// TODO(viettrungluu): Test Close(). crbug.com/69457
