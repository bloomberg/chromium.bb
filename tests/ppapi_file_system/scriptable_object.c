/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_request_info.h"

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) { \
      return PP_MakeBool(PP_FALSE); \
    } \
  } while (0)

/* Function prototypes */
static struct PPB_Var_Deprecated* GetPPB_Var();
static struct PPB_Core* GetPPB_Core();
static struct PP_Var TestCreate(struct PPB_FileSystem_Dev* ppb_file_system);
static struct PP_Var TestIsFileSystem(
    struct PPB_FileSystem_Dev* ppb_file_system);
static struct PP_Var TestOpen(struct PPB_FileSystem_Dev* ppb_file_system);
static struct PP_Var TestGetType(struct PPB_FileSystem_Dev* ppb_file_system);

/* Global variables */
PPB_GetInterface get_browser_interface_func = NULL;
PP_Instance instance = 0;
PP_Module module = 0;
const PP_Instance kInvalidInstance = 0;
const PP_Resource kInvalidResource = 0;
const PP_FileSystemType_Dev kFileSystemTypes[] = {
  PP_FILESYSTEMTYPE_EXTERNAL,
  PP_FILESYSTEMTYPE_LOCALPERSISTENT,
  PP_FILESYSTEMTYPE_LOCALTEMPORARY
};
const size_t kNumFileSystemTypes =
    sizeof(kFileSystemTypes) / sizeof(kFileSystemTypes[0]);

static bool HasProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var* exception) {
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* property_name = ppb_var_interface->VarToUtf8(name, &len);

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);

  return (0 == strncmp(property_name, "__moduleReady", len));
}

static bool HasMethod(void* object,
                      struct PP_Var name,
                      struct PP_Var* exception) {
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* method_name = ppb_var_interface->VarToUtf8(name, &len);
  bool has_method = false;

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);

  if (0 == strncmp(method_name, "testCreate", len) ||
      0 == strncmp(method_name, "testIsFileSystem", len) ||
      0 == strncmp(method_name, "testOpen", len) ||
      0 == strncmp(method_name, "testGetType", len)) {
    has_method = true;
  }
  return has_method;
}

static struct PP_Var GetProperty(void* object,
                                 struct PP_Var name,
                                 struct PP_Var* exception) {
  struct PP_Var var = PP_MakeUndefined();
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* property_name = ppb_var_interface->VarToUtf8(name, &len);

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);

  if (0 == strncmp(property_name, "__moduleReady", len)) {
    var = PP_MakeInt32(1);
  }
  return var;
}

static void GetAllPropertyNames(void* object,
                                uint32_t* property_count,
                                struct PP_Var** properties,
                                struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(properties);
  UNREFERENCED_PARAMETER(exception);

  *property_count = 0;
}

static void SetProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var value,
                        struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(name);
  UNREFERENCED_PARAMETER(value);
  UNREFERENCED_PARAMETER(exception);
}

static void RemoveProperty(void* object,
                           struct PP_Var name,
                           struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(name);
  UNREFERENCED_PARAMETER(exception);
}

static struct PP_Var Call(void* object,
                          struct PP_Var method_name,
                          uint32_t argc,
                          struct PP_Var* argv,
                          struct PP_Var* exception) {
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  uint32_t len;
  const char* method = ppb_var_interface->VarToUtf8(method_name, &len);
  static struct PPB_FileSystem_Dev* ppb_file_system = NULL;

  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);
  UNREFERENCED_PARAMETER(exception);

  if (NULL == ppb_file_system) {
    ppb_file_system = (struct PPB_FileSystem_Dev*)(*get_browser_interface_func)(
        PPB_FILESYSTEM_DEV_INTERFACE);
  }
  if (NULL == ppb_file_system) {
    printf("%s ppb_file_system is NULL.\n", __FUNCTION__);
    printf("\tPPB_FILESYSTEM_DEV_INTERFACE: %s.\n",
           PPB_FILESYSTEM_DEV_INTERFACE);
    return PP_MakeNull();
  }
  if (0 == strncmp(method, "testCreate", len)) {
    return TestCreate(ppb_file_system);
  } else if (0 == strncmp(method, "testIsFileSystem", len)) {
    return TestIsFileSystem(ppb_file_system);
  } else if (0 == strncmp(method, "testOpen", len)) {
    return TestOpen(ppb_file_system);
  } else if (0 == strncmp(method, "testGetType", len)) {
    return TestGetType(ppb_file_system);
  }
  return PP_MakeNull();
}

static struct PP_Var Construct(void* object,
                               uint32_t argc,
                               struct PP_Var* argv,
                               struct PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);
  UNREFERENCED_PARAMETER(exception);
  return PP_MakeUndefined();
}

static void Deallocate(void* object) {
  UNREFERENCED_PARAMETER(object);
}

struct PP_Var GetScriptableObject(PP_Instance plugin_instance) {
  if (NULL != get_browser_interface_func) {
    struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
    static struct PPP_Class_Deprecated ppp_class = {
      HasProperty,
      HasMethod,
      GetProperty,
      GetAllPropertyNames,
      SetProperty,
      RemoveProperty,
      Call,
      Construct,
      Deallocate
    };
    instance = plugin_instance;

    return ppb_var_interface->CreateObject(instance, &ppp_class, NULL);
  }
  return PP_MakeNull();
}

static struct PPB_Core* GetPPB_Core() {
  return (struct PPB_Core*)(*get_browser_interface_func)(
      PPB_CORE_INTERFACE);
}

static struct PPB_Var_Deprecated* GetPPB_Var() {
  return (struct PPB_Var_Deprecated*)(*get_browser_interface_func)(
      PPB_VAR_DEPRECATED_INTERFACE);
}

static struct PP_Var TestCreate(struct PPB_FileSystem_Dev* ppb_file_system) {
  PP_Resource file_system = 0;
  struct PPB_Core* const ppb_core = GetPPB_Core();
  /*
   * Test to see if PPB_FileSystem_Dev::Create returns PP_Resource value of 0
   * if the instance parameter is invalid.
   */
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_EXTERNAL);
  EXPECT_TRUE(file_system == kInvalidResource);
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_LOCALPERSISTENT);
  EXPECT_TRUE(file_system == kInvalidResource);
  file_system = ppb_file_system->Create(kInvalidInstance,
                                        PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  EXPECT_TRUE(file_system == kInvalidResource);

  /* Test for failure when an invalid file system type is requested. */
  file_system = ppb_file_system->Create(instance, PP_FILESYSTEMTYPE_INVALID);
  EXPECT_TRUE(file_system == kInvalidResource);

  /*
   * Test to see if PPB_FileSystem_Dev::Create returns a valid PP_Resource
   * value when given a valid PP_Instance value parameter.  Test for all
   * three file system types PPB_FileSystem_Dev supports.
   */
  for (size_t j = 0; j < kNumFileSystemTypes; ++j) {
    file_system =
        ppb_file_system->Create(instance, kFileSystemTypes[j]);
    EXPECT_TRUE(file_system != kInvalidResource);
    ppb_core->ReleaseResource(file_system);
  }
  return PP_MakeBool(PP_TRUE);
}

void OpenCallback(void* data, int32_t result) {
  /*
   * We don't check for the result value since we only care the callback was
   * fired.
   */
  struct PPB_Var_Deprecated* ppb_var = GetPPB_Var();
  struct PP_Var window;
  struct PP_Var open_callback = ppb_var->VarFromUtf8(
      module,
      "OpenCallback",
      strlen("OpenCallback"));
  struct PP_Var exception = PP_MakeUndefined();
  struct PPB_Instance* instance_interface =
      (struct PPB_Instance*)(*get_browser_interface_func)(
          PPB_INSTANCE_INTERFACE);

  UNREFERENCED_PARAMETER(data);
  UNREFERENCED_PARAMETER(result);

  if (NULL == instance_interface)
    printf("%s instance_interface is NULL\n", __FUNCTION__);
  window = instance_interface->GetWindowObject(instance);
  if (PP_VARTYPE_OBJECT != window.type) {
    printf("%s window.type: %i\n", __FUNCTION__, window.type);
  }
  ppb_var->Call(window, open_callback, 0, NULL, &exception);
  ppb_var->Release(open_callback);
  ppb_var->Release(window);
}

static struct PP_Var TestIsFileSystem(
    struct PPB_FileSystem_Dev* ppb_file_system) {
  struct PPB_Core* const ppb_core = GetPPB_Core();
  struct PPB_URLRequestInfo* const ppb_url_request_info =
      (struct PPB_URLRequestInfo*)(*get_browser_interface_func)(
          PPB_URLREQUESTINFO_INTERFACE);
  PP_Resource url_request_info = 0;
  PP_Resource file_system = 0;
  size_t j = 0;
  PP_Bool is_file_system = PP_FALSE;

  /* Test fail for invalid resource. */
  EXPECT_TRUE(ppb_file_system->IsFileSystem(kInvalidResource) != PP_TRUE);

  /*
   * Test pass for the different valid system types, and test fail against a
   * resource that has been released.
   */
  for (j = 0; j < kNumFileSystemTypes; ++j) {
    file_system = ppb_file_system->Create(instance,
                                          kFileSystemTypes[j]);
    CHECK(file_system != 0);

    is_file_system = ppb_file_system->IsFileSystem(file_system);
    ppb_core->ReleaseResource(file_system);

    EXPECT_TRUE(is_file_system == PP_TRUE);

    is_file_system = ppb_file_system->IsFileSystem(file_system);
    EXPECT_TRUE(is_file_system == PP_FALSE);
  }

  /* Test fail against a non-filesystem resource */
  url_request_info = ppb_url_request_info->Create(instance);
  CHECK(url_request_info != 0);
  is_file_system = ppb_file_system->IsFileSystem(url_request_info);
  ppb_core->ReleaseResource(url_request_info);
  EXPECT_TRUE(is_file_system == PP_FALSE);

  return PP_MakeBool(PP_TRUE);
}

static struct PP_Var TestOpen(struct PPB_FileSystem_Dev* ppb_file_system) {
  size_t j = 0;
  PP_Resource file_system = 0;
  struct PP_CompletionCallback open_callback = PP_MakeCompletionCallback(
      OpenCallback,
      NULL);
  struct PPB_Core* const ppb_core = GetPPB_Core();

  /* Test to make sure opening an invalid file system fails. */
  int32_t pp_error = ppb_file_system->Open(kInvalidResource,
                                           1024,  /* Dummy value */
                                           open_callback);
  EXPECT_TRUE(pp_error == PP_ERROR_BADRESOURCE);

  /*
   * Test to make sure external file system is not supported.
   * TODO(sanga): Once Chrome supports external file systems, change this test
   * to reflect the change.
   */
  file_system = ppb_file_system->Create(instance,
                                        PP_FILESYSTEMTYPE_EXTERNAL);
  pp_error = ppb_file_system->Open(file_system,
                                   1024,  /* Dummy value */
                                   open_callback);
  ppb_core->ReleaseResource(file_system);
  EXPECT_TRUE(pp_error != PP_OK);
  EXPECT_TRUE(pp_error != PP_OK_COMPLETIONPENDING);

  /* Test local temporary and local persistant file systems */
  for (j = 1; j < kNumFileSystemTypes; ++j) {
#ifdef __native_client__
    /* Test fail for blocking open */
    /*
     * Only conduct this test with nexe.  Trusted ppapi plugin does not work
     * with synchronous Open call.
     * See http://code.google.com/p/chromium/issues/detail?id=78449
     */
    file_system = ppb_file_system->Create(instance,
                                          kFileSystemTypes[j]);
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     PP_BlockUntilComplete());
    ppb_core->ReleaseResource(file_system);
    EXPECT_TRUE(pp_error == PP_ERROR_BADARGUMENT);
#endif

    /* Test success for asynchronous open */
    file_system = ppb_file_system->Create(instance,
                                          kFileSystemTypes[j]);
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     open_callback);
    ppb_core->ReleaseResource(file_system);
    EXPECT_TRUE(pp_error == PP_OK_COMPLETIONPENDING);

    /* Test fail for multiple opens */
    file_system = ppb_file_system->Create(instance,
                                          kFileSystemTypes[j]);
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     open_callback);
    CHECK(pp_error == PP_OK_COMPLETIONPENDING);  /* Previously tested */
    pp_error = ppb_file_system->Open(file_system,
                                     1024,  /* Dummy value */
                                     open_callback);
    ppb_core->ReleaseResource(file_system);
    EXPECT_TRUE(pp_error ==  PP_ERROR_FAILED);
  }

  return PP_MakeBool(PP_TRUE);
}

static struct PP_Var TestGetType(struct PPB_FileSystem_Dev* ppb_file_system) {
  struct PPB_Core* const ppb_core = GetPPB_Core();
  struct PPB_URLRequestInfo* const ppb_url_request_info =
      (struct PPB_URLRequestInfo*)(*get_browser_interface_func)(
          PPB_URLREQUESTINFO_INTERFACE);
  PP_Resource url_request_info = 0;
  PP_Resource file_system = 0;
  size_t j = 0;
  PP_FileSystemType_Dev type = PP_FILESYSTEMTYPE_INVALID;

  /* Test for invalid resource. */
  if (PP_FILESYSTEMTYPE_INVALID != ppb_file_system->GetType(0))
    return PP_MakeBool(PP_FALSE);

  /* Test pass for the different valid system types */
  for (j = 0; j < kNumFileSystemTypes; ++j) {
    file_system = ppb_file_system->Create(instance,
                                          kFileSystemTypes[j]);
    CHECK(file_system != 0);

    type = ppb_file_system->GetType(file_system);
    ppb_core->ReleaseResource(file_system);

    EXPECT_TRUE(type == kFileSystemTypes[j]);
  }

  /* Test fail against a non-filesystem resource */
  url_request_info = ppb_url_request_info->Create(instance);
  CHECK(url_request_info != 0);

  type = ppb_file_system->GetType(url_request_info);
  ppb_core->ReleaseResource(url_request_info);
  EXPECT_TRUE(type == PP_FILESYSTEMTYPE_INVALID);

  return PP_MakeBool(PP_TRUE);
}
