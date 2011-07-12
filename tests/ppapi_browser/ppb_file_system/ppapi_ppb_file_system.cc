// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/pp_errors.h"

namespace {

void OpenCallback(void* /*data*/, int32_t /*result*/) {
}

const PP_FileSystemType_Dev kFileSystemTypes[] = {
  PP_FILESYSTEMTYPE_EXTERNAL,
  PP_FILESYSTEMTYPE_LOCALPERSISTENT,
  PP_FILESYSTEMTYPE_LOCALTEMPORARY
};

const size_t kNumFileSystemTypes =
    sizeof(kFileSystemTypes) / sizeof(kFileSystemTypes[0]);

void TestCreate() {
  PP_Resource file_system = kInvalidResource;
  const struct PPB_Core* const ppb_core = PPBCore();
  const struct PPB_FileSystem_Dev* const ppb_file_system = PPBFileSystemDev();
  /*
   * Test to see if PPB_FileSystem_Dev::Create returns PP_Resource value of 0
   * if the instance parameter is invalid.
   */
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_EXTERNAL);
  EXPECT(file_system == kInvalidResource);
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  EXPECT(file_system == kInvalidResource);
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  EXPECT(file_system == kInvalidResource);

  /* Test for failure when an invalid file system type is requested. */
  file_system = ppb_file_system->Create(pp_instance(),
                                        PP_FILESYSTEMTYPE_INVALID);
  EXPECT(file_system == kInvalidResource);

  /*
   * Test to see if PPB_FileSystem_Dev::Create returns a valid PP_Resource
   * value when given a valid PP_Instance value parameter.  Test for all
   * three file system types PPB_FileSystem_Dev supports.
   */
  for (size_t j = 0; j < kNumFileSystemTypes; ++j) {
    file_system =
        ppb_file_system->Create(pp_instance(), kFileSystemTypes[j]);
    EXPECT(file_system != kInvalidResource);
    ppb_core->ReleaseResource(file_system);
  }
  TEST_PASSED;
}

void TestIsFileSystem() {
  const struct PPB_Core* const ppb_core = PPBCore();
  const struct PPB_FileSystem_Dev* const ppb_file_system = PPBFileSystemDev();
  const struct PPB_URLRequestInfo* const ppb_url_request_info =
      PPBURLRequestInfo();
  PP_Resource url_request_info = kInvalidResource;
  PP_Resource file_system = kInvalidResource;
  size_t j = 0;
  PP_Bool is_file_system = PP_FALSE;

  /* Test fail for invalid resource. */
  EXPECT(ppb_file_system->IsFileSystem(kInvalidResource) != PP_TRUE);

  /*
   * Test pass for the different valid system types, and test fail against a
   * resource that has been released.
   */
  for (j = 0; j < kNumFileSystemTypes; ++j) {
    file_system = ppb_file_system->Create(pp_instance(),
                                          kFileSystemTypes[j]);
    CHECK(file_system != kInvalidResource);

    is_file_system = ppb_file_system->IsFileSystem(file_system);
    ppb_core->ReleaseResource(file_system);

    EXPECT(is_file_system == PP_TRUE);

    is_file_system = ppb_file_system->IsFileSystem(file_system);
    EXPECT(is_file_system == PP_FALSE);
  }

  /* Test fail against a non-filesystem resource */
  url_request_info = ppb_url_request_info->Create(pp_instance());
  CHECK(url_request_info != kInvalidResource);
  is_file_system = ppb_file_system->IsFileSystem(url_request_info);
  ppb_core->ReleaseResource(url_request_info);
  EXPECT(is_file_system == PP_FALSE);

  TEST_PASSED;
}

void TestOpen() {
  size_t j = 0;
  PP_Resource file_system = 0;
  const struct PP_CompletionCallback open_callback =
      MakeTestableCompletionCallback(
          "OpenCallback",
          OpenCallback,
          NULL);  // user data
  const struct PPB_Core* const ppb_core = PPBCore();
  const struct PPB_FileSystem_Dev* const ppb_file_system = PPBFileSystemDev();

  /* Test to make sure opening an invalid file system fails. */
  int32_t pp_error = ppb_file_system->Open(kInvalidResource,
                                           1024,  /* Dummy value */
                                           open_callback);
  EXPECT(pp_error == PP_ERROR_BADRESOURCE);

  /*
   * Test to make sure external file system is not supported.
   * TODO(sanga): Once Chrome supports external file systems, change this test
   * to reflect the change.
   */
  file_system = ppb_file_system->Create(pp_instance(),
                                        PP_FILESYSTEMTYPE_EXTERNAL);
  pp_error = ppb_file_system->Open(file_system,
                                   1024,  /* Dummy value */
                                   open_callback);
  ppb_core->ReleaseResource(file_system);
  EXPECT(pp_error != PP_OK);
  EXPECT(pp_error != PP_OK_COMPLETIONPENDING);

  /* Test local temporary and local persistant file systems */
  for (j = 1; j < kNumFileSystemTypes; ++j) {
#ifdef __native_client__
    /* Test fail for blocking open */
    /*
     * Only conduct this test with nexe.  Trusted ppapi plugin does not work
     * with synchronous Open call.
     * See http://code.google.com/p/chromium/issues/detail?id=78449
     */
    file_system = ppb_file_system->Create(pp_instance(),
                                          kFileSystemTypes[j]);
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     PP_BlockUntilComplete());
    ppb_core->ReleaseResource(file_system);
    EXPECT(pp_error == PP_ERROR_BADARGUMENT);
#endif

    /* Test success for asynchronous open */
    file_system = ppb_file_system->Create(pp_instance(),
                                          kFileSystemTypes[j]);
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     open_callback);
    ppb_core->ReleaseResource(file_system);
    EXPECT(pp_error == PP_OK_COMPLETIONPENDING);

    /* Test fail for multiple opens */
    file_system = ppb_file_system->Create(pp_instance(),
                                          kFileSystemTypes[j]);
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     open_callback);
    CHECK(pp_error == PP_OK_COMPLETIONPENDING);  /* Previously tested */
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     open_callback);
    ppb_core->ReleaseResource(file_system);
    EXPECT(pp_error ==  PP_ERROR_FAILED);
  }
  TEST_PASSED;
}

void TestGetType() {
  const struct PPB_Core* const ppb_core = PPBCore();
  const struct PPB_FileSystem_Dev* const ppb_file_system = PPBFileSystemDev();
  const struct PPB_URLRequestInfo* const ppb_url_request_info =
      PPBURLRequestInfo();
  PP_Resource url_request_info = kInvalidResource;
  PP_Resource file_system = kInvalidResource;
  size_t j = 0;
  PP_FileSystemType_Dev type = PP_FILESYSTEMTYPE_INVALID;

  /* Test for invalid resource. */
  EXPECT(PP_FILESYSTEMTYPE_INVALID == ppb_file_system->GetType(0));

  /* Test pass for the different valid system types */
  for (j = 0; j < kNumFileSystemTypes; ++j) {
    file_system = ppb_file_system->Create(pp_instance(),
                                          kFileSystemTypes[j]);
    CHECK(file_system != kInvalidResource);

    type = ppb_file_system->GetType(file_system);
    ppb_core->ReleaseResource(file_system);

    EXPECT(type == kFileSystemTypes[j]);
  }

  /* Test fail against a non-filesystem resource */
  url_request_info = ppb_url_request_info->Create(pp_instance());
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
