/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */




#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include "native_client/src/include/base/basictypes.h"
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
