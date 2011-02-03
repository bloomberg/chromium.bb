/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

/* Function prototypes */
static struct PPB_Var_Deprecated* GetPPB_Var();
static struct PP_Var TestAddRefAndReleaseResource(struct PPB_Core* core);
static struct PP_Var TestAddRefAndReleaseInvalidResource(struct PPB_Core* core);

/* Prototypes of function implemenataions of the PPP_Class_Deprecated
 * interface. */
static bool HasProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var* exception);
static bool HasMethod(void* object,
                      struct PP_Var name,
                      struct PP_Var* exception);
static struct PP_Var GetProperty(void* object,
                                 struct PP_Var name,
                                 struct PP_Var* exception);
static void GetAllPropertyNames(void* object,
                                uint32_t* property_count,
                                struct PP_Var** properties,
                                struct PP_Var* exception);
static void SetProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var value,
                        struct PP_Var* exception);
static void RemoveProperty(void* object,
                           struct PP_Var name,
                           struct PP_Var* exception);
static struct PP_Var Call(void* object,
                          struct PP_Var method_name,
                          uint32_t argc,
                          struct PP_Var* argv,
                          struct PP_Var* exception);
static struct PP_Var Construct(void* object,
                               uint32_t argc,
                               struct PP_Var* argv,
                               struct PP_Var* exception);
static void Deallocate(void* object);

/* Global variables */
PPB_GetInterface get_browser_interface_func = NULL;
PP_Instance instance = 0;
PP_Module module = 0;
struct PPP_Class_Deprecated ppp_class = {
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

static bool HasProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var* exception) {
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* property_name = ppb_var_interface->VarToUtf8(name, &len);
  return (0 == strncmp(property_name, "__moduleReady", len));
}

static bool HasMethod(void* object,
                      struct PP_Var name,
                      struct PP_Var* exception) {
  uint32_t len = 0;
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  const char* method_name = ppb_var_interface->VarToUtf8(name, &len);
  bool has_method = false;
  if (0 == strncmp(method_name, "testAddRefAndReleaseResource", len) ||
      0 == strncmp(method_name, "testAddRefAndReleaseInvalidResource", len) ||
      0 == strncmp(method_name, "testCallOnMainThread", len) ||
      0 == strncmp(method_name, "testGetTime", len) ||
      0 == strncmp(method_name, "testGetTimeTicks", len) ||
      0 == strncmp(method_name, "testIsMainThread", len) ||
      0 == strncmp(method_name, "testMemAllocAndMemFree", len)) {
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
  if (0 == strncmp(property_name, "__moduleReady", len)) {
    var = PP_MakeInt32(1);
  }
  return var;
}

static void GetAllPropertyNames(void* object,
                                uint32_t* property_count,
                                struct PP_Var** properties,
                                struct PP_Var* exception) {
  *property_count = 0;
}

static void SetProperty(void* object,
                        struct PP_Var name,
                        struct PP_Var value,
                        struct PP_Var* exception) {
}

static void RemoveProperty(void* object,
                           struct PP_Var name,
                           struct PP_Var* exception) {
}

void CompletionCallback(void* data, int32_t result) {
  struct PPB_Instance* instance_interface =
      (struct PPB_Instance*)(*get_browser_interface_func)(
          PPB_INSTANCE_INTERFACE);
  if (NULL == instance_interface)
    printf("%s instance_interface is NULL\n", __FUNCTION__);
  struct PPB_Var_Deprecated* ppb_var = GetPPB_Var();
  struct PP_Var window = instance_interface->GetWindowObject(instance);
  if (PP_VARTYPE_OBJECT != window.type) {
    printf("%s window.type: %i\n", __FUNCTION__, window.type);
  }
  struct PP_Var call_on_main_thread_callback = ppb_var->VarFromUtf8(
      module,
      "CallOnMainThreadCallback",
      strlen("CallOnMainThreadCallback"));
  struct PP_Var exception = PP_MakeUndefined();
  ppb_var->Call(window, call_on_main_thread_callback, 0, NULL, &exception);
  ppb_var->Release(call_on_main_thread_callback);
  ppb_var->Release(window);
}

static struct PP_Var Call(void* object,
                          struct PP_Var method_name,
                          uint32_t argc,
                          struct PP_Var* argv,
                          struct PP_Var* exception) {
  struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
  uint32_t len;
  const char* method = ppb_var_interface->VarToUtf8(method_name, &len);
  static struct PPB_Core* core = NULL;
  if (NULL == core)
    core = (struct PPB_Core*)(*get_browser_interface_func)(PPB_CORE_INTERFACE);
  if (0 == strncmp(method, "testAddRefAndReleaseResource", len)) {
    /* Test PPB_Core::AddRefResource and PPB_Core::ReleaseResource */
    return TestAddRefAndReleaseResource(core);
  } else if (0 == strncmp(method, "testAddRefAndReleaseInvalidResource", len)) {
    /* Test PPB_Core::AddRefResource and PPB_Core::ReleaseResource with invalid
     * resource value.
     */
    return TestAddRefAndReleaseInvalidResource(core);
  } else if (0 == strncmp(method, "testGetTime", len)) {
    /* Test PPB_Core::GetTime */
    PP_Time time1 = core->GetTime();
    usleep(100000);  /* 0.1 second */
    PP_Time time2 = core->GetTime();
    PP_Bool time_passed = (time2 > time1) ? PP_TRUE : PP_FALSE;
    return PP_MakeBool(time_passed);
  } else if (0 == strncmp(method, "testGetTimeTicks", len)) {
    /* Test PPB_Core::GetTimeTicks */
    PP_TimeTicks time_ticks1 = core->GetTimeTicks();
    usleep(100000);  /* 0.1 second */
    PP_TimeTicks time_ticks2 = core->GetTimeTicks();
    PP_Bool time_passed = (time_ticks2 > time_ticks1) ? PP_TRUE : PP_FALSE;
    return PP_MakeBool(time_passed);
  } else if (0 == strncmp(method, "testCallOnMainThread", len)) {
    /* Test PPB_Core::CallOnMainThread */
    core->CallOnMainThread(0,  /* delay */
                           PP_MakeCompletionCallback(CompletionCallback,
                                                     NULL),  /* no user data */
                           0);  /* dummy result */
    return PP_MakeBool(PP_TRUE);
  } else if (0 == strncmp(method, "testIsMainThread", len)) {
    /* Test PPB_Core::IsOnMainThread */
    if (PP_TRUE == core->IsMainThread())
      return PP_MakeBool(PP_TRUE);
    return PP_MakeBool(PP_FALSE);
  } else if (0 == strncmp(method, "testMemAllocAndMemFree", len)) {
    /* Test PPB_Core::MemAlloc and PPB_Core::MemFree */
    void* mem = core->MemAlloc(100);  /* No signficance to using 100 */
    core->MemFree(mem);
    return PP_MakeBool(PP_TRUE);
  }
  return PP_MakeNull();
}

static struct PP_Var Construct(void* object,
                               uint32_t argc,
                               struct PP_Var* argv,
                               struct PP_Var* exception) {
  return PP_MakeUndefined();
}

static void Deallocate(void* object) {
}

struct PP_Var GetScriptableObject(PP_Instance plugin_instance) {
  if (NULL != get_browser_interface_func) {
    instance = plugin_instance;
    struct PPB_Var_Deprecated* ppb_var_interface = GetPPB_Var();
    return ppb_var_interface->CreateObject(instance, &ppp_class, NULL);
  }
  return PP_MakeNull();
}

static struct PP_Var TestAddRefAndReleaseResource(struct PPB_Core* core) {
  /*
   * As a test, instantiate an url request resource, add to its reference count,
   * and then release it the resource.
   */
  struct PPB_URLRequestInfo* ppb_url_request_info =
      (struct PPB_URLRequestInfo*)(*get_browser_interface_func)(
          PPB_URLREQUESTINFO_INTERFACE);
  PP_Resource url_request_info = ppb_url_request_info->Create(instance);
  if (PP_FALSE == ppb_url_request_info->IsURLRequestInfo(url_request_info))
    return PP_MakeBool(PP_FALSE);
  /*
   * Url request resource is already implemented and tested.  Return value of 0
   * indicates an invalid module and is not expected.
   */
  if (0 != url_request_info) {
    /* Add reference count and release multiple times. */
    for (size_t j = 0; j < 10; ++j) core->AddRefResource(url_request_info);
    for (size_t j = 0; j < 10; ++j) core->ReleaseResource(url_request_info);
    /* Make sure this is still a valid url request info (refcount is not 0) */
    if (PP_FALSE == ppb_url_request_info->IsURLRequestInfo(url_request_info))
      return PP_MakeBool(PP_FALSE);
    core->ReleaseResource(url_request_info);  /* Release count from Create() */
    /* Make sure the url request info is properly deallocated. */
    if (PP_FALSE == ppb_url_request_info->IsURLRequestInfo(url_request_info))
      return PP_MakeBool(PP_TRUE);
    return PP_MakeBool(PP_FALSE);
  }
  return PP_MakeBool(PP_FALSE);
}

static struct PP_Var TestAddRefAndReleaseInvalidResource(
    struct PPB_Core* core) {
  /*
   * As a test, attempt to increment the reference count of an invalid
   * PP_Resource as well as decrement the reference count.
   */
  /* Add reference count and release multiple times. */
  for (size_t j = 0; j < 10; ++j) {
    core->AddRefResource(0);
    core->ReleaseResource(0);
  }
  return PP_MakeBool(PP_TRUE);
}

static struct PPB_Var_Deprecated* GetPPB_Var() {
  return (struct PPB_Var_Deprecated*)(*get_browser_interface_func)(
      PPB_VAR_DEPRECATED_INTERFACE);
}
