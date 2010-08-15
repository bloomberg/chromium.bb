/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/portable_handle.h"
#include <stdio.h>
#include <string.h>
#include <map>

#include "native_client/src/trusted/plugin/browser_interface.h"

namespace plugin {

PortableHandle::PortableHandle() {
  PLUGIN_PRINTF(("PortableHandle::PortableHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

PortableHandle::~PortableHandle() {
  PLUGIN_PRINTF(("PortableHandle::~PortableHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

void PortableHandle::AddPropertyGet(RpcFunction function_ptr,
                                    const char* name,
                                    const char* outs) {
  uintptr_t method_id = browser_interface()->StringToIdentifier(name);
  PLUGIN_PRINTF(("PortableHandle::AddPropertyGet (name='%s', id=%"
                 NACL_PRIxPTR")\n", name, method_id));
  MethodInfo* new_method =
      new(std::nothrow) MethodInfo(function_ptr, name, "", outs);
  if (NULL == new_method) {
    return;
  }
  property_get_methods_.AddMethod(method_id, new_method);
}

void PortableHandle::AddPropertySet(RpcFunction function_ptr,
                                    const char* name,
                                    const char* ins) {
  uintptr_t method_id = browser_interface()->StringToIdentifier(name);
  PLUGIN_PRINTF(("PortableHandle::AddPropertySet (name='%s', id=%"
                 NACL_PRIxPTR")\n", name, method_id));
  MethodInfo* new_method =
      new(std::nothrow) MethodInfo(function_ptr, name, ins, "");
  if (NULL == new_method) {
    return;
  }
  property_set_methods_.AddMethod(method_id, new_method);
}

void PortableHandle::AddMethodCall(RpcFunction function_ptr,
                                   const char* name,
                                   const char* ins,
                                   const char* outs) {
  uintptr_t method_id = browser_interface()->StringToIdentifier(name);
  PLUGIN_PRINTF(("PortableHandle::AddMethodCall (name='%s', id=%"
                 NACL_PRIxPTR")\n", name, method_id));
  MethodInfo* new_method =
      new(std::nothrow) MethodInfo(function_ptr, name, ins, outs);
  if (NULL == new_method) {
    return;
  }
  methods_.AddMethod(method_id, new_method);
}

bool PortableHandle::InitParams(uintptr_t method_id,
                                CallType call_type,
                                SrpcParams* params) {
  MethodInfo* method_info = GetMethodInfo(method_id, call_type);
  if (NULL == method_info) {
    return InitParamsEx(method_id, call_type, params);
  } else {
    return params->Init(method_info->ins(), method_info->outs());
  }
}

MethodInfo* PortableHandle::GetMethodInfo(uintptr_t method_id,
                                          CallType call_type) {
  MethodInfo* method_info = NULL;
  switch (call_type) {
    case METHOD_CALL:
      method_info = methods_.GetMethod(method_id);
      break;
    case PROPERTY_GET:
      method_info = property_get_methods_.GetMethod(method_id);
      break;
    case PROPERTY_SET:
      method_info = property_set_methods_.GetMethod(method_id);
      break;
  }
  PLUGIN_PRINTF(("PortableHandle::GetMethodInfo (id=%"NACL_PRIxPTR", "
                 "return %p)\n", method_id, static_cast<void*>(method_info)));
  return method_info;
}

bool PortableHandle::HasMethod(uintptr_t method_id, CallType call_type) {
  if (GetMethodInfo(method_id, call_type)) {
    return true;
  }
  // Call class-specific implementation - see ConnectedSocket and Plugin
  // classes for examples.
  return HasMethodEx(method_id, call_type);
}

// TODO(gregoryd) - consider adding a function that will first initialize
// params and then call Invoke
bool PortableHandle::Invoke(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params) {
  MethodInfo* method_info = GetMethodInfo(method_id, call_type);

  if (NULL != method_info && NULL != method_info->function_ptr()) {
    return method_info->function_ptr()(reinterpret_cast<void*>(this), params);
  } else {
    // This should be handled by the class-specific handler
    return InvokeEx(method_id, call_type, params);
  }
}

bool PortableHandle::InitParamsEx(uintptr_t method_id,
                                  CallType call_type,
                                  SrpcParams* params) {
  UNREFERENCED_PARAMETER(method_id);
  UNREFERENCED_PARAMETER(call_type);
  UNREFERENCED_PARAMETER(params);
  return false;
}

bool PortableHandle::InvokeEx(uintptr_t method_id,
                              CallType call_type,
                              SrpcParams* params) {
  UNREFERENCED_PARAMETER(method_id);
  UNREFERENCED_PARAMETER(call_type);
  UNREFERENCED_PARAMETER(params);
  return false;
}

bool PortableHandle::HasMethodEx(uintptr_t method_id, CallType call_type) {
  UNREFERENCED_PARAMETER(method_id);
  UNREFERENCED_PARAMETER(call_type);
  return false;
}

}  // namespace plugin
