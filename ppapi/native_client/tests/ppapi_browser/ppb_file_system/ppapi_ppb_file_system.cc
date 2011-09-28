// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/pp_errors.h"

namespace {

struct OpenCallbackData {
  explicit OpenCallbackData(PP_Resource file_system)
      : file_system(file_system) {}

  PP_Resource file_system;
};

void OpenFailCallback(void* data, int32_t result) {
  EXPECT(result == PP_ERROR_NOACCESS);
  OpenCallbackData* callback_data = reinterpret_cast<OpenCallbackData*>(data);
  PPBCore()->ReleaseResource(callback_data->file_system);
  delete callback_data;
}

void OpenCallback(void* data, int32_t result) {
  EXPECT(result == PP_OK);
  OpenCallbackData* callback_data = reinterpret_cast<OpenCallbackData*>(data);
  PPBCore()->ReleaseResource(callback_data->file_system);
  delete callback_data;
}

const PP_FileSystemType kFileSystemTypes[] = {
  PP_FILESYSTEMTYPE_EXTERNAL,
  PP_FILESYSTEMTYPE_LOCALPERSISTENT,
  PP_FILESYSTEMTYPE_LOCALTEMPORARY
};

const size_t kNumFileSystemTypes =
    sizeof(kFileSystemTypes) / sizeof(kFileSystemTypes[0]);

void TestCreate() {
  PP_Resource file_system = kInvalidResource;
  const PPB_FileSystem* ppb_file_system = PPBFileSystem();
  // Test to see if PPB_FileSystem::Create returns PP_Resource value of 0
  // if the instance parameter is invalid.
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_EXTERNAL);
  EXPECT(file_system == kInvalidResource);
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  EXPECT(file_system == kInvalidResource);
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  EXPECT(file_system == kInvalidResource);

  // Test for failure when an invalid file system type is requested.
  file_system = ppb_file_system->Create(pp_instance(),
                                        PP_FILESYSTEMTYPE_INVALID);
  EXPECT(file_system == kInvalidResource);

  // Test to see if PPB_FileSystem::Create returns a valid PP_Resource
  // value when given a valid PP_Instance value parameter.  Test for all
  // three file system types PPB_FileSystem supports.
  for (size_t i = 0; i < kNumFileSystemTypes; ++i) {
    file_system = ppb_file_system->Create(pp_instance(), kFileSystemTypes[i]);
    EXPECT(file_system != kInvalidResource);
    PPBCore()->ReleaseResource(file_system);
  }
  TEST_PASSED;
}

void TestIsFileSystem() {
  const PPB_Core* ppb_core = PPBCore();
  const PPB_FileSystem* ppb_file_system = PPBFileSystem();
  PP_Resource file_system = kInvalidResource;
  PP_Bool is_file_system = PP_FALSE;

  // Test fail for invalid resource.
  EXPECT(ppb_file_system->IsFileSystem(kInvalidResource) != PP_TRUE);

  // Test pass for the different valid system types, and test fail against a
  // resource that has been released.
  for (size_t i = 0; i < kNumFileSystemTypes; ++i) {
    file_system = ppb_file_system->Create(pp_instance(), kFileSystemTypes[i]);
    CHECK(file_system != kInvalidResource);

    is_file_system = ppb_file_system->IsFileSystem(file_system);
    ppb_core->ReleaseResource(file_system);

    EXPECT(is_file_system == PP_TRUE);

    is_file_system = ppb_file_system->IsFileSystem(file_system);
    EXPECT(is_file_system == PP_FALSE);
  }

  // Test fail against a non-filesystem resource.
  PP_Resource url_request_info = PPBURLRequestInfo()->Create(pp_instance());
  CHECK(url_request_info != kInvalidResource);
  is_file_system = ppb_file_system->IsFileSystem(url_request_info);
  ppb_core->ReleaseResource(url_request_info);
  EXPECT(is_file_system == PP_FALSE);

  TEST_PASSED;
}

void TestOpen() {
  const PPB_FileSystem* ppb_file_system = PPBFileSystem();
  static PP_Resource file_system = kInvalidResource;
  PP_CompletionCallback nop_callback =
      MakeTestableCompletionCallback("NopCallback", OpenCallback);
  int32_t pp_error = PP_ERROR_FAILED;
  int64_t kSize = 1024;  // Dummy value.

  // Test to make sure opening an invalid file system fails.
  pp_error = ppb_file_system->Open(kInvalidResource, kSize, nop_callback);
  EXPECT(pp_error == PP_ERROR_BADRESOURCE);

  // Test that external storage fails with permission error.
  file_system = ppb_file_system->Create(pp_instance(),
                                        PP_FILESYSTEMTYPE_EXTERNAL);
  OpenCallbackData* callback_data_ext = new OpenCallbackData(file_system);
  PP_CompletionCallback open_fail_callback = MakeTestableCompletionCallback(
      "OpenCallback", OpenFailCallback, callback_data_ext);
  pp_error = ppb_file_system->Open(file_system, kSize, open_fail_callback);
  EXPECT(pp_error == PP_OK_COMPLETIONPENDING);

  // Test local temporary and local persistant file systems.
  for (size_t i = 1; i < kNumFileSystemTypes; ++i) {
#ifdef __native_client__
    // Test fail for blocking open.
    //
    // Only conduct this test with nexe.  Trusted ppapi plugin does not work
    // with synchronous Open call.
    // See http://code.google.com/p/chromium/issues/detail?id=78449
    file_system = ppb_file_system->Create(pp_instance(), kFileSystemTypes[i]);
    pp_error = ppb_file_system->Open(file_system, kSize,
                                     PP_BlockUntilComplete());
    ppb_core->ReleaseResource(file_system);
    EXPECT(pp_error == PP_ERROR_BLOCKS_MAIN_THREAD);
#endif

    // Test success for asynchronous open.
    file_system = ppb_file_system->Create(pp_instance(), kFileSystemTypes[i]);
    OpenCallbackData* callback_data = new OpenCallbackData(file_system);
    PP_CompletionCallback open_callback = MakeTestableCompletionCallback(
        "OpenCallback", OpenCallback, callback_data);
    pp_error = ppb_file_system->Open(file_system, kSize, open_callback);
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);

    // Test fail for multiple opens.
    file_system = ppb_file_system->Create(pp_instance(), kFileSystemTypes[i]);
    callback_data = new OpenCallbackData(file_system);
    open_callback = MakeTestableCompletionCallback(
        "OpenCallback", OpenCallback, callback_data);
    pp_error = ppb_file_system->Open(file_system, kSize, open_callback);
    CHECK(pp_error == PP_OK_COMPLETIONPENDING);  // Previously tested.
    pp_error = ppb_file_system->Open(file_system, kSize, nop_callback);
    EXPECT(pp_error == PP_ERROR_INPROGRESS);
  }
  TEST_PASSED;
}

void TestGetType() {
  const PPB_Core* ppb_core = PPBCore();
  const PPB_FileSystem* ppb_file_system = PPBFileSystem();
  PP_Resource file_system = kInvalidResource;
  PP_FileSystemType type = PP_FILESYSTEMTYPE_INVALID;

  // Test for invalid resource.
  EXPECT(PP_FILESYSTEMTYPE_INVALID == ppb_file_system->GetType(0));

  // Test pass for the different valid system types.
  for (size_t i = 0; i < kNumFileSystemTypes; ++i) {
    file_system = ppb_file_system->Create(pp_instance(), kFileSystemTypes[i]);
    CHECK(file_system != kInvalidResource);

    type = ppb_file_system->GetType(file_system);
    ppb_core->ReleaseResource(file_system);

    EXPECT(type == kFileSystemTypes[i]);
  }

  // Test fail against a non-filesystem resource.
  PP_Resource url_request_info = PPBURLRequestInfo()->Create(pp_instance());
  CHECK(url_request_info != kInvalidResource);
  type = ppb_file_system->GetType(url_request_info);
  ppb_core->ReleaseResource(url_request_info);
  EXPECT(type == PP_FILESYSTEMTYPE_INVALID);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsFileSystem", TestIsFileSystem);
  RegisterTest("TestOpen", TestOpen);
  RegisterTest("TestGetType", TestGetType);
}

void SetupPluginInterfaces() {
  /* No PPP interface to test. */
}
