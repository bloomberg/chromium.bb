// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/nacl_subprocess.h"

#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/service_runtime.h"

namespace plugin {

nacl::string NaClSubprocess::description() {
  nacl::stringstream ss;
  if (assigned_id_ == kMainSubprocessId) {
    ss << "main subprocess";
  } else {
    ss << "helper subprocess #" << assigned_id_;
  }
  return ss.str();
}

nacl::string NaClSubprocess::detailed_description() {
  nacl::stringstream ss;
  ss << description()
     << " w/ this(" << static_cast<void*>(this)
     << "), socket(" << static_cast<void*>(socket_)
     << "), service_runtime(" << static_cast<void*>(service_runtime_)
     << ")";
  return ss.str();
}

// Shutdown the socket connection and service runtime, in that order.
void NaClSubprocess::Shutdown() {
  ScriptableHandle::Unref(&socket_);
  if (service_runtime_ != NULL) {
    service_runtime_->Shutdown();
    delete service_runtime_;
    service_runtime_ = NULL;
  }
}

NaClSubprocess::~NaClSubprocess() {
  Shutdown();
}

bool NaClSubprocess::StartSrpcServices() {
  ScriptableHandle::Unref(&socket_);
  socket_ = service_runtime_->SetupAppChannel();
  return socket_ != NULL;
}

bool NaClSubprocess::StartJSObjectProxy(Plugin* plugin, ErrorInfo* error_info) {
  return socket_->handle()->StartJSObjectProxy(plugin, error_info);
}

bool NaClSubprocess::HasMethod(uintptr_t method_id, CallType call_type) {
  if (NULL == socket()) {
    return false;
  }
  return socket()->handle()->HasMethod(method_id, call_type);
}

bool NaClSubprocess::InitParams(uintptr_t method_id,
                                CallType call_type,
                                SrpcParams* params) {
  if (NULL == socket()) {
    return false;
  }
  return socket()->handle()->InitParams(method_id, call_type, params);
}

bool NaClSubprocess::Invoke(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params) {
  if (NULL == socket()) {
    return false;
  }
  return socket()->handle()->Invoke(method_id, call_type, params);
}

}  // namespace plugin
