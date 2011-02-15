// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_file_io.h"

#include <stdio.h>
#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/cpp/dev/file_io_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FileIO);

namespace {

std::string ReportMismatch(const std::string& method_name,
                           const std::string& returned_result,
                           const std::string& expected_result) {
  return method_name + " returned '" + returned_result + "'; '" +
      expected_result + "' expected.";
}

int32_t ReadEntireFile(PP_Instance instance,
                       pp::FileIO_Dev* file_io,
                       int32_t offset,
                       std::string* data) {
  TestCompletionCallback callback(instance);
  char buf[256];
  int32_t read_offset = offset;

  for (;;) {
    int32_t rv = file_io->Read(read_offset, buf, sizeof(buf), callback);
    if (rv == PP_ERROR_WOULDBLOCK)
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

int32_t WriteEntireBuffer(PP_Instance instance,
                          pp::FileIO_Dev* file_io,
                          int32_t offset,
                          const std::string& data) {
  TestCompletionCallback callback(instance);
  int32_t write_offset = offset;
  const char* buf = data.c_str();
  int32_t size = data.size();

  while (write_offset < offset + size) {
    int32_t rv = file_io->Write(write_offset, &buf[write_offset - offset],
                                size - write_offset + offset, callback);
    if (rv == PP_ERROR_WOULDBLOCK)
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
  return InitTestingInterface() && EnsureRunningOverHTTP();
}

void TestFileIO::RunTest() {
  RUN_TEST(Open);
  RUN_TEST(ReadWriteSetLength);
  RUN_TEST(TouchQuery);
  RUN_TEST(AbortCalls);
  // TODO(viettrungluu): add tests:
  //  - that PP_ERROR_PENDING is correctly returned
  //  - that operations respect the file open modes (flags)
}

std::string TestFileIO::TestOpen() {
  TestCompletionCallback callback(instance_->pp_instance());

  pp::FileSystem_Dev file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef_Dev file_ref(file_system, "/file_open");
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO_Dev file_io(instance_);
  rv = file_io.Open(file_ref, PP_FILEOPENFLAG_CREATE, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Try opening a file that doesn't exist.
  pp::FileRef_Dev nonexistent_file_ref(file_system, "/nonexistent_file");
  pp::FileIO_Dev nonexistent_file_io(instance_);
  rv = nonexistent_file_io.Open(
      nonexistent_file_ref, PP_FILEOPENFLAG_READ, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_ERROR_FILENOTFOUND)
    return ReportError("FileIO::Open", rv);

  PASS();
}

std::string TestFileIO::TestReadWriteSetLength() {
  TestCompletionCallback callback(instance_->pp_instance());

  pp::FileSystem_Dev file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef_Dev file_ref(file_system, "/file_read_write_setlength");
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileIO_Dev file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE |
                    PP_FILEOPENFLAG_READ |
                    PP_FILEOPENFLAG_WRITE,
                    callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Write something to the file.
  rv = WriteEntireBuffer(instance_->pp_instance(), &file_io, 0, "test_test");
  if (rv != PP_OK)
    return ReportError("FileIO::Write", rv);

  // Read the entire file.
  std::string read_buffer;
  rv = ReadEntireFile(instance_->pp_instance(), &file_io, 0, &read_buffer);
  if (rv != PP_OK)
    return ReportError("FileIO::Read", rv);
  if (read_buffer != "test_test")
    return ReportMismatch("FileIO::Read", read_buffer, "test_test");

  // Truncate the file.
  rv = file_io.SetLength(4, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
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
  rv = file_io.SetLength(16, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
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

std::string TestFileIO::TestTouchQuery() {
  TestCompletionCallback callback(instance_->pp_instance());

  pp::FileSystem_Dev file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  pp::FileRef_Dev file_ref(file_system, "/file_touch");
  pp::FileIO_Dev file_io(instance_);
  rv = file_io.Open(file_ref,
                    PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
                    callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileIO::Open", rv);

  // Write some data to have a non-zero file size.
  rv = file_io.Write(0, "test", 4, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != 4)
    return ReportError("FileIO::Write", rv);

  // last_access_time's granularity is 1 day
  // last_modified_time's granularity is 2 seconds
  const PP_Time last_access_time = 123 * 24 * 3600.0;
  const PP_Time last_modified_time = 246.0;
  rv = file_io.Touch(last_access_time, last_modified_time, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Touch", rv);

  PP_FileInfo_Dev info;
  rv = file_io.Query(&info, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Query", rv);

  if ((info.size != 4) ||
      (info.type != PP_FILETYPE_REGULAR) ||
      (info.system_type != PP_FILESYSTEMTYPE_LOCALTEMPORARY) ||
      (info.last_access_time != last_access_time) ||
      (info.last_modified_time != last_modified_time))
    return "FileSystem::Query() has returned bad data.";

  // Call |Query()| again, to make sure it works a second time.
  rv = file_io.Query(&info, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Query", rv);

  PASS();
}

std::string TestFileIO::TestAbortCalls() {
  TestCompletionCallback callback(instance_->pp_instance());

  pp::FileSystem_Dev file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  pp::FileRef_Dev file_ref(file_system, "/file_abort_calls");
  int32_t rv = file_system.Open(1024, callback);
  if (rv == PP_ERROR_WOULDBLOCK)
    rv = callback.WaitForResult();
  if (rv != PP_OK)
    return ReportError("FileSystem::Open", rv);

  // First, create a file which to do ops on.
  {
    pp::FileIO_Dev file_io(instance_);
    rv = file_io.Open(file_ref,
                      PP_FILEOPENFLAG_CREATE | PP_FILEOPENFLAG_WRITE,
                      callback);
    if (rv == PP_ERROR_WOULDBLOCK)
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
    rv = pp::FileIO_Dev(instance_)
        .Open(file_ref, PP_FILEOPENFLAG_READ,callback);
    if (callback.run_count() > 0)
      return "FileIO::Open ran callback synchronously.";
    if (rv == PP_ERROR_WOULDBLOCK) {
      rv = callback.WaitForResult();
      if (rv != PP_ERROR_ABORTED)
        return "FileIO::Open not aborted.";
    } else if (rv != PP_OK) {
      return ReportError("FileIO::Open", rv);
    }
  }

  // Abort |Query()|.
  {
    PP_FileInfo_Dev info = { 0 };
    {
      pp::FileIO_Dev file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Query(&info, callback);
    }  // Destroy |file_io|.
    if (rv == PP_ERROR_WOULDBLOCK) {
      // Save a copy and make sure |info| doesn't get written to.
      PP_FileInfo_Dev info_copy;
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
      pp::FileIO_Dev file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_WRITE, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Touch(0, 0, callback);
    }  // Destroy |file_io|.
    if (rv == PP_ERROR_WOULDBLOCK) {
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
      pp::FileIO_Dev file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Read(0, buf, sizeof(buf), callback);
    }  // Destroy |file_io|.
    if (rv == PP_ERROR_WOULDBLOCK) {
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
      pp::FileIO_Dev file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Write(0, buf, sizeof(buf), callback);
    }  // Destroy |file_io|.
    if (rv == PP_ERROR_WOULDBLOCK) {
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
      pp::FileIO_Dev file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.SetLength(3, callback);
    }  // Destroy |file_io|.
    if (rv == PP_ERROR_WOULDBLOCK) {
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
      pp::FileIO_Dev file_io(instance_);
      rv = file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback);
      if (rv == PP_ERROR_WOULDBLOCK)
        rv = callback.WaitForResult();
      if (rv != PP_OK)
        return ReportError("FileIO::Open", rv);

      callback.reset_run_count();
      rv = file_io.Flush(callback);
    }  // Destroy |file_io|.
    if (rv == PP_ERROR_WOULDBLOCK) {
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

// TODO(viettrungluu): Test Close(). crbug.com/69457
