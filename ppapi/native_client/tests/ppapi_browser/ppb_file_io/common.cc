// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/native_client/tests/ppapi_browser/ppb_file_io/common.h"

#include <string.h>
#include <string>

#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"

namespace common {

const char* kTestData =
    "Everywhere is within walking distance if you have the time";

void InitFileInfo(PP_FileSystemType system_type, PP_FileInfo* file_info) {
  memset(file_info, 0, sizeof(PP_FileInfo));
  file_info->system_type = system_type;
  file_info->type = PP_FILETYPE_REGULAR;
  file_info->last_access_time = 200;  // dummy value
  file_info->last_modified_time = 100;  // something less than last access time
}

BoundPPAPIFunc OpenFileForTest::GetCompletionCallbackInitiatingPPAPIFunction(
    TestCallbackData* callback_data) {
  return std::tr1::bind(PPBFileIO()->Open,
                        callback_data->existing_file_io,
                        callback_data->existing_file_ref,
                        PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE,
                        std::tr1::placeholders::_1);
}

void FileIOTester::AddSequenceElement(TestSequenceElement* element) {
  test_sequence_.push_back(element);
}

void FileIOTester::Run() {
  if (test_sequence_.empty())
    return;

  PP_Resource file_system = PPBFileSystem()->Create(pp_instance(),
                                                    file_info_.system_type);
  EXPECT(file_system != kInvalidResource);

  TestCallbackData* callback_data = new TestCallbackData(file_system,
                                                         file_info_,
                                                         test_sequence_);

  int32_t pp_error = PPBFileSystem()->Open(
      callback_data->file_system,
      1024,
      MakeTestableCompletionCallback(
          "OpenFileSystemForSetupCallback",
          OpenFileSystemForSetupCallback,
          callback_data));
  EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
}

void FileIOTester::FlushFileForSetupCallback(void* data, int32_t result) {
  // This callback should be preceded by a write operation.
  EXPECT(result >= PP_OK);

  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  PP_FileInfo file_info = callback_data->file_info;
  int32_t pp_error = PPBFileIO()->Touch(callback_data->existing_file_io,
                                        file_info.last_access_time,
                                        file_info.last_modified_time,
                                        MakeTestableCompletionCallback(
                                            "TouchFileForSetupCallback",
                                            TouchFileForSetupCallback,
                                            callback_data));
  EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
}

// This is the first callback in the chain of callbacks.  The first several in
// the chain are initialization for the tests that come later in the chain.
void FileIOTester::OpenFileSystemForSetupCallback(void* data, int32_t result) {
  EXPECT(result == PP_OK);

  // Need to retrieve file system from the data in order to create file ref
  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  const PP_Resource file_system = callback_data->file_system;
  // Create file ref for non-existing file
  callback_data->non_existing_file_ref = PPBFileRef()->Create(
      file_system,
      "/non_existing_file");
  EXPECT(callback_data->non_existing_file_ref != kInvalidResource);

  // Create file io for non-existing file
  callback_data->non_existing_file_io = PPBFileIO()->Create(pp_instance());
  EXPECT(callback_data->non_existing_file_io != kInvalidResource);

  // Create file ref for existing file
  callback_data->existing_file_ref = PPBFileRef()->Create(file_system,
                                                          "/existing_file");
  EXPECT(callback_data->existing_file_ref != kInvalidResource);

  // Create file io for existing file
  callback_data->existing_file_io = PPBFileIO()->Create(pp_instance());
  EXPECT(callback_data->existing_file_io != kInvalidResource);

  const PP_FileInfo& file_info = callback_data->file_info;
  if (file_info.type == PP_FILETYPE_REGULAR) {
    int32_t pp_error = PPBFileIO()->Open(callback_data->existing_file_io,
                                         callback_data->existing_file_ref,
                                         PP_FILEOPENFLAG_TRUNCATE |
                                         PP_FILEOPENFLAG_CREATE |
                                         PP_FILEOPENFLAG_WRITE,
                                         MakeTestableCompletionCallback(
                                             "OpenFileForSetupCallback",
                                             OpenFileForSetupCallback,
                                             callback_data));
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
  } else if (file_info.type == PP_FILETYPE_DIRECTORY) {
    // TODO(sanga): Log a message indicating directories are not yet supported
    // in these tests.
  }
}

void FileIOTester::OpenFileForSetupCallback(void* data, int32_t result) {
  EXPECT(result == PP_OK);

  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  PP_FileInfo file_info = callback_data->file_info;
  int32_t pp_error = PPBFileIO()->Write(callback_data->existing_file_io,
                                        0,  // no offset
                                        kTestData,
                                        strlen(kTestData),
                                        MakeTestableCompletionCallback(
                                            "WriteFileForSetupCallback",
                                            WriteFileForSetupCallback,
                                            callback_data));
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
}

void FileIOTester::StartTestSequence(TestCallbackData* callback_data) {
  if (!callback_data->test_sequence.empty()) {
    TestSequenceElement* first_test_element =
        callback_data->test_sequence.front();
    PP_CompletionCallback callback = MakeTestableCompletionCallback(
        first_test_element->name(),
        TestSequenceElement::TestSequenceElementForwardingCallback,
        callback_data);
    PPBCore()->CallOnMainThread(0,  // no delay
                                callback, PP_OK);
  } else {
    TestSequenceElement::CleanupResources(callback_data);
  }
}

// Assign testing file properties with data from given callback data.
void FileIOTester::TouchFileForSetupCallback(void* data, int32_t result) {
  EXPECT(result == PP_OK);

  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  PPBFileIO()->Close(callback_data->existing_file_io);
  StartTestSequence(callback_data);
}

void FileIOTester::WriteFileForSetupCallback(void* data, int32_t result) {
  EXPECT(result >= PP_OK);

  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  int32_t pp_error = PPBFileIO()->Flush(callback_data->existing_file_io,
                                        MakeTestableCompletionCallback(
                                            "FlushFileForSetupCallback",
                                            FlushFileForSetupCallback,
                                            callback_data));
  EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
}

}  // namespace common
