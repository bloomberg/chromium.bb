/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <string.h>

#include <map>

#include "base/basictypes.h"
#include "native_client/src/trusted/plugin/srpc/portable_handle.h"

namespace nacl_srpc {

  int PortableHandle::number_alive_counter = 0;


  PortableHandle::PortableHandle() : plugin_interface_(NULL) {
    dprintf(("PortableHandle::PortableHandle(%p, %d)\n",
             static_cast<void *>(this),
             ++number_alive_counter));
  }

  PortableHandle::~PortableHandle() {
    dprintf(("PortableHandle::~PortableHandle(%p, %d)\n",
             static_cast<void *>(this),
             --number_alive_counter));
  }

  void PortableHandle::AddMethodToMap(RpcFunction function_ptr,
                                      const char *name,
                                      CallType call_type,
                                      const char *ins,
                                      const char *outs) {
    uintptr_t method_id =
        PortablePluginInterface::GetStrIdentifierCallback(name);
    MethodInfo* new_method = new(std::nothrow) MethodInfo(function_ptr,
                                                          name,
                                                          ins,
                                                          outs);
    if (NULL == new_method) {
      return;
    }
    switch (call_type) {
      case METHOD_CALL:
        methods_.AddMethod(method_id, new_method);
        break;
      case PROPERTY_GET:
        property_get_methods_.AddMethod(method_id, new_method);
        break;
      case PROPERTY_SET:
        property_set_methods_.AddMethod(method_id, new_method);
        break;
      default:
        dprintf(("PortableHandle::AddMethodToMap(%p) - unknown call type %d\n",
                 static_cast<void *>(this), call_type));
    }
  }

  bool PortableHandle::Init(struct PortableHandleInitializer* init_info)  {
    plugin_interface_ = init_info->plugin_interface_;
    return true;
  }

  bool PortableHandle::InitParams(uintptr_t method_id,
                                  CallType call_type,
                                  SrpcParams *params) {
    MethodInfo *method_info = GetMethodInfo(method_id, call_type);
    if (method_info) {
      return params->Init(method_info->ins_, method_info->outs_);
    } else {
      return InitParamsEx(method_id, call_type, params);
    }
  }

  bool PortableHandle::InitParamsEx(uintptr_t method_id,
                                    CallType call_type,
                                    SrpcParams *params) {
    UNREFERENCED_PARAMETER(method_id);
    UNREFERENCED_PARAMETER(call_type);
    UNREFERENCED_PARAMETER(params);
    return false;
  }

  MethodInfo* PortableHandle::GetMethodInfo(uintptr_t method_id,
                                            CallType call_type) {
    MethodInfo *method_info = NULL;
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
      default:
        dprintf(("PortableHandle::Invoke(%p) - unknown call type %d\n",
                 static_cast<void *>(this), call_type));
    }

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
    MethodInfo *method_info = GetMethodInfo(method_id, call_type);

    if (method_info) {
      return method_info->function_ptr_(reinterpret_cast<void*>(this), params);
    } else {
      // This should be handled by the class-specific handler
      return InvokeEx(method_id, call_type, params);
    }
  }

  bool PortableHandle::InvokeEx(uintptr_t method_id,
                                CallType call_type,
                                SrpcParams *params) {
    UNREFERENCED_PARAMETER(method_id);
    UNREFERENCED_PARAMETER(call_type);
    params->SetExceptionInfo("Unrecognized method specified");
    return false;
  }

  bool PortableHandle::HasMethodEx(uintptr_t method_id, CallType call_type) {
    UNREFERENCED_PARAMETER(method_id);
    UNREFERENCED_PARAMETER(call_type);
    return false;
  }
}
