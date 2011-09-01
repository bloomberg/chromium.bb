// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/native_client/tests/ppapi_browser/ppb_file_io/common.h"

namespace {

using common::FileIOTester;
using common::InitFileInfo;
using common::kTestData;
using common::OpenFileForTest;
using common::TestCallbackData;
using common::TestSequenceElement;

struct TestReadData {
  TestReadData(PP_Resource file_io, int64_t offset_bytes, int32_t bytes_to_read)
      : file_io(file_io), buffer(new char[bytes_to_read]),
      offset(offset_bytes), bytes_to_read(bytes_to_read), bytes_read(0) { }
  ~TestReadData() {
    delete[] buffer;
  }

  const PP_Resource file_io;
  char* const buffer;
  const int64_t offset;
  const int32_t bytes_to_read;
  int32_t bytes_read;
};

// Completion callback function called from the read operation.  If the read
// operation is not completed, a successive read operation will be invoked.
void ReadCallback(void* data, int32_t bytes_read) {
  EXPECT(bytes_read >= 0);

  TestReadData* read_data = reinterpret_cast<TestReadData*>(data);
  read_data->bytes_read += bytes_read;

  if (read_data->bytes_read == read_data->bytes_to_read ||
      bytes_read == 0) {  // no more bytes to available to read
    // Verify the data
    EXPECT(strncmp(kTestData + read_data->offset, read_data->buffer,
                   read_data->bytes_read) == 0);
    PostTestMessage(__FUNCTION__, "VERIFIED");  // Test for this in browser.
    delete read_data;
  } else {
    int64_t offset = read_data->offset + read_data->bytes_read;
    char* buffer = read_data->buffer + read_data->bytes_read;
    int32_t bytes_to_read =
        read_data->bytes_to_read - read_data->bytes_read;
    PPBFileIO()->Read(read_data->file_io, offset, buffer, bytes_to_read,
                      PP_MakeCompletionCallback(ReadCallback, read_data));
  }
}

// TestParallelRead performs two read operations, one from the beginning of the
// file, and one from the offset.  Both operations are of the same size.
class TestParallelRead : public TestSequenceElement {
 public:
  TestParallelRead(int64_t offset, int32_t bytes_to_read)
      : TestSequenceElement("TestParallelRead", PP_OK_COMPLETIONPENDING, PP_OK),
      offset_(offset), bytes_to_read_(bytes_to_read) {}

 private:
  virtual void Setup(TestCallbackData* callback_data) {
    TestReadData* read_data = new TestReadData(callback_data->existing_file_io,
                                               0,  // read from beginning
                                               bytes_to_read_);
    int32_t pp_error = PPBFileIO()->Read(read_data->file_io, read_data->offset,
                                         read_data->buffer,
                                         read_data->bytes_to_read,
                                         PP_MakeCompletionCallback(ReadCallback,
                                                                   read_data));
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);

    read_data = new TestReadData(callback_data->existing_file_io, offset_,
                                 bytes_to_read_);
    pp_error = PPBFileIO()->Read(read_data->file_io, read_data->offset,
                                 read_data->buffer,
                                 read_data->bytes_to_read,
                                 PP_MakeCompletionCallback(ReadCallback,
                                                           read_data));
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
  }

  const int32_t offset_;
  const int32_t bytes_to_read_;
};

class TestFileRead : public TestSequenceElement {
 public:
  TestFileRead(int64_t offset, int32_t bytes_to_read)
      : TestSequenceElement("TestFileRead", PP_OK_COMPLETIONPENDING, PP_OK),
      offset_(offset), bytes_to_read_(bytes_to_read) {}

 private:
  void Setup(TestCallbackData* callback_data) {
    TestReadData* read_data = new TestReadData(callback_data->existing_file_io,
                                               offset_, bytes_to_read_);
    int32_t pp_error = PPBFileIO()->Read(read_data->file_io, read_data->offset,
                                         read_data->buffer,
                                         read_data->bytes_to_read,
                                         PP_MakeCompletionCallback(ReadCallback,
                                                                   read_data));
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
  }

  const int64_t offset_;
  const int32_t bytes_to_read_;
};

// Test the partial read of a file
void TestPartialFileRead(PP_FileSystemType system_type) {
  PP_FileInfo file_info = { 0 };
  InitFileInfo(system_type, &file_info);

  const int64_t offset = 1;  // some non-zero offset
  const int32_t bytes_to_read = strlen(kTestData) / 2;

  FileIOTester tester(file_info);
  tester.AddSequenceElement(new OpenFileForTest);
  tester.AddSequenceElement(new TestFileRead(offset, bytes_to_read));
  tester.Run();
}

// Test complete read of a file in one operation.
void TestCompleteReadFile(PP_FileSystemType system_type) {
  PP_FileInfo file_info = { 0 };
  InitFileInfo(system_type, &file_info);

  FileIOTester tester(file_info);
  tester.AddSequenceElement(new OpenFileForTest);
  tester.AddSequenceElement(new TestFileRead(0,  // start at beginning
                                             strlen(kTestData)));
  tester.Run();
}

void TestParallelReadFile(PP_FileSystemType system_type) {
  PP_FileInfo file_info = { 0 };
  InitFileInfo(system_type, &file_info);

  const int64_t offset = strlen(kTestData) / 2;
  const int32_t bytes_to_read = strlen(kTestData) / 3;

  FileIOTester tester(file_info);
  tester.AddSequenceElement(new OpenFileForTest);
  tester.AddSequenceElement(new TestParallelRead(offset, bytes_to_read));
  tester.Run();
}

}  // namespace

void TestPartialFileReadLocalTemporary() {
  TestPartialFileRead(PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  TEST_PASSED;
}

void TestCompleteReadLocalTemporary() {
  TestCompleteReadFile(PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  TEST_PASSED;
}

// Test multiple reads of a file using multiple callbacks and different offsets.
void TestParallelReadLocalTemporary() {
  TestParallelReadFile(PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  TEST_PASSED;
}
