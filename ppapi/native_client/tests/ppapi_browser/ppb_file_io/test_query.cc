// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"
#include "ppapi/native_client/tests/ppapi_browser/ppb_file_io/common.h"

namespace {

using common::BoundPPAPIFunc;
using common::FileIOTester;
using common::InitFileInfo;
using common::kTestData;
using common::OpenFileForTest;
using common::TestCallbackData;
using common::TestSequenceElement;

class TestQuery : public TestSequenceElement {
 public:
  TestQuery() : TestSequenceElement("TestQuery") {}

 private:
  virtual BoundPPAPIFunc GetCompletionCallbackInitiatingPPAPIFunction(
      TestCallbackData* callback_data) {
    PP_FileInfo* file_info = new PP_FileInfo;
    callback_data->data = file_info;
    return std::tr1::bind(PPBFileIO()->Query, callback_data->existing_file_io,
                          file_info, std::tr1::placeholders::_1);
  }
};

// Verify the results of calling PPB_FileIO:::Query in from a TestQuery object.
class TestQueryFileVerify : public TestSequenceElement {
 public:
  TestQueryFileVerify() : TestSequenceElement("TestQueryFileVerify") {}

 private:
  virtual void Execute(TestCallbackData* callback_data) {
    PP_FileInfo* file_info =
        reinterpret_cast<PP_FileInfo*>(callback_data->data);
    EXPECT(file_info->type == callback_data->file_info.type);
    EXPECT(file_info->system_type == callback_data->file_info.system_type);
    // TODO(sanga): Fix this with the correct test.
    // EXPECT(file_info->last_access_time ==
    //        callback_data->file_info.last_access_time);
    EXPECT(file_info->last_modified_time ==
           callback_data->file_info.last_modified_time);
    EXPECT(file_info->size == strlen(kTestData));
    delete file_info;
  }
};

void TestQueryFile(PP_FileSystemType system_type) {
  PP_FileInfo file_info = { 0 };
  InitFileInfo(system_type, &file_info);
  FileIOTester tester(file_info);
  tester.AddSequenceElement(new OpenFileForTest);
  tester.AddSequenceElement(new TestQuery);
  tester.AddSequenceElement(new TestQueryFileVerify);
  tester.Run();
}

}  // namespace

void TestQueryFileLocalTemporary() {
  TestQueryFile(PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  TEST_PASSED;
}
