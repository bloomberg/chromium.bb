// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/native_client/tests/ppapi_browser/ppb_file_io/common.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"

namespace {

using common::BoundPPAPIFunc;
using common::FileIOTester;
using common::InitFileInfo;
using common::TestCallbackData;
using common::TestSequenceElement;

class TestOpenExistingFileSequenceElement : public TestSequenceElement {
 public:
  TestOpenExistingFileSequenceElement(const std::string& name,
                                      int32_t expected_return_value,
                                      int32_t expected_result, int32_t flags)
      : TestSequenceElement(name, expected_return_value, expected_result),
      flags_(flags) {}
  virtual ~TestOpenExistingFileSequenceElement() {}

 private:
  virtual BoundPPAPIFunc GetCompletionCallbackInitiatingPPAPIFunction(
      TestCallbackData* callback_data) {
    return std::tr1::bind(PPBFileIO()->Open, callback_data->existing_file_io,
                          callback_data->existing_file_ref, flags_,
                          std::tr1::placeholders::_1);
  }
  virtual void Setup(TestCallbackData* callback_data) {
    // We make sure the file io is closed before each test.
    PPBFileIO()->Close(callback_data->existing_file_io);
  }

  const int32_t flags_;
};

class TestOpenNonExistingFileSequenceElement : public TestSequenceElement {
 public:
  TestOpenNonExistingFileSequenceElement(const std::string& name,
                                         int32_t expected_return_value,
                                         int32_t expected_result, int32_t flags)
      : TestSequenceElement(name, expected_return_value, expected_result),
      flags_(flags) {}
  virtual ~TestOpenNonExistingFileSequenceElement() {}

 private:
  virtual BoundPPAPIFunc GetCompletionCallbackInitiatingPPAPIFunction(
      TestCallbackData* callback_data) {
    return std::tr1::bind(PPBFileIO()->Open,
                          callback_data->non_existing_file_io,
                          callback_data->non_existing_file_ref, flags_,
                          std::tr1::placeholders::_1);
  }
  virtual void Setup(TestCallbackData* callback_data) {
    // We make sure the file io is closed before each test.
    PPBFileIO()->Close(callback_data->non_existing_file_io);
  }

  const int32_t flags_;
};

class DeleteFile : public TestSequenceElement {
 public:
  DeleteFile()
      : TestSequenceElement("DeleteFile", PP_OK_COMPLETIONPENDING, PP_OK) {}

 private:
  virtual BoundPPAPIFunc GetCompletionCallbackInitiatingPPAPIFunction(
      TestCallbackData* callback_data) {
    return std::tr1::bind(PPBFileRef()->Delete,
                          callback_data->non_existing_file_ref,
                          std::tr1::placeholders::_1);
  }
};

void TestOpenNonExistingFile(PP_FileSystemType system_type) {
  PP_FileInfo file_info = { 0 };
  InitFileInfo(system_type, &file_info);

  FileIOTester tester(file_info);
  tester.AddSequenceElement(
      new TestOpenNonExistingFileSequenceElement("TestOpenForRead",
                                                 PP_OK_COMPLETIONPENDING,
                                                 PP_ERROR_FILENOTFOUND,
                                                 PP_FILEOPENFLAG_READ));
  tester.AddSequenceElement(
      new TestOpenNonExistingFileSequenceElement("TestOpenForWrite",
                                                 PP_OK_COMPLETIONPENDING,
                                                 PP_ERROR_FILENOTFOUND,
                                                 PP_FILEOPENFLAG_WRITE));
  tester.AddSequenceElement(
      new TestOpenNonExistingFileSequenceElement("TestOpenForWriteCreate",
                                                 PP_OK_COMPLETIONPENDING,
                                                 PP_OK,
                                                 PP_FILEOPENFLAG_CREATE |
                                                 PP_FILEOPENFLAG_WRITE));
  tester.AddSequenceElement(new DeleteFile);
  tester.AddSequenceElement(
      new TestOpenNonExistingFileSequenceElement(
          "TestOpenForWriteCreateExclusive",
          PP_OK_COMPLETIONPENDING,
          PP_OK,
          PP_FILEOPENFLAG_WRITE |
          PP_FILEOPENFLAG_CREATE |
          PP_FILEOPENFLAG_EXCLUSIVE));
  tester.Run();
}

void TestOpenExistingFile(PP_FileSystemType system_type) {
  PP_FileInfo file_info = { 0 };
  InitFileInfo(system_type, &file_info);

  FileIOTester tester(file_info);
  tester.AddSequenceElement(
      new TestOpenExistingFileSequenceElement("TestOpenForRead",
                                             PP_OK_COMPLETIONPENDING, PP_OK,
                                             PP_FILEOPENFLAG_READ));
          tester.AddSequenceElement(
      new TestOpenExistingFileSequenceElement("TestOpenForWrite",
                                              PP_OK_COMPLETIONPENDING, PP_OK,
                                              PP_FILEOPENFLAG_WRITE));
  tester.AddSequenceElement(
      new TestOpenExistingFileSequenceElement("TestOpenTruncate",
                                              PP_ERROR_BADARGUMENT,
                                              PP_ERROR_FAILED,
                                              PP_FILEOPENFLAG_TRUNCATE));
  tester.AddSequenceElement(
      new TestOpenExistingFileSequenceElement("TestOpenForWriteCreateExclusive",
                                              PP_OK_COMPLETIONPENDING,
                                              PP_ERROR_FILEEXISTS,
                                              PP_FILEOPENFLAG_WRITE |
                                              PP_FILEOPENFLAG_CREATE |
                                              PP_FILEOPENFLAG_EXCLUSIVE));
  tester.AddSequenceElement(
      new TestOpenExistingFileSequenceElement("TestOpenForWriteCreate",
                                              PP_OK_COMPLETIONPENDING, PP_OK,
                                              PP_FILEOPENFLAG_WRITE |
                                              PP_FILEOPENFLAG_CREATE));
  tester.Run();
}

}  // namespace

void TestOpenExistingFileLocalTemporary() {
  TestOpenExistingFile(PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  TEST_PASSED;
}

void TestOpenNonExistingFileLocalTemporary() {
  TestOpenNonExistingFile(PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  TEST_PASSED;
}
