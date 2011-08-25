// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NATIVE_CLIENT_TESTS_PPAPI_BROWSER_PPB_FILE_IO_COMMON_H_
#define NATIVE_CLIENT_TESTS_PPAPI_BROWSER_PPB_FILE_IO_COMMON_H_

#include <deque>

#include "native_client/src/third_party/ppapi/c/pp_file_info.h"
#include "native_client/tests/ppapi_browser/ppb_file_io/test_sequence_element.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

namespace common {

extern const char* kTestData;

// Open file for subsequent tests.
class OpenFileForTest : public TestSequenceElement {
 public:
  OpenFileForTest()
      : TestSequenceElement("OpenFileForTest") {}

 private:
  virtual BoundPPAPIFunc GetCompletionCallbackInitiatingPPAPIFunction(
      TestCallbackData* callback_data);
};

// Initializes the member data of given file info to either known values, (e.g.
// file system type), or dummy values (e.g. last modified time).
void InitFileInfo(PP_FileSystemType system_type, PP_FileInfo* file_info);

// TODO(sanga): Move this to file_io_tester.h
// FileIOTester is the test runner.  Used to accrue a sequence of test elements
// in a specific order and run the sequence.
class FileIOTester {
 public:
  explicit FileIOTester(const PP_FileInfo& file_info)
      : file_info_(file_info) {}
  ~FileIOTester() {}

  void AddSequenceElement(TestSequenceElement* element);  // sink
  void Run();

 private:
  // Callbacks for setting up before executing the test sequence.
  static void FlushFileForSetupCallback(void* data, int32_t result);
  static void OpenFileForSetupCallback(void* data, int32_t result);
  static void OpenFileSystemForSetupCallback(void* data, int32_t result);
  static void StartTestSequence(TestCallbackData* callback_data);
  static void TouchFileForSetupCallback(void* data, int32_t result);
  static void WriteFileForSetupCallback(void* data, int32_t result);

  const PP_FileInfo file_info_;
  std::deque<TestSequenceElement*> test_sequence_;

  DISALLOW_COPY_AND_ASSIGN(FileIOTester);
};

}  // namespace common

#endif  // NATIVE_CLIENT_TESTS_PPAPI_BROWSER_PPB_FILE_IO_COMMON_H_
