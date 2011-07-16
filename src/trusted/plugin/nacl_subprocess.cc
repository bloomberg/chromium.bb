// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/nacl_subprocess.h"

#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/service_runtime.h"

namespace plugin {

nacl::string NaClSubprocess::description() const {
  nacl::stringstream ss;
  if (assigned_id_ == kMainSubprocessId) {
    ss << "main subprocess";
  } else {
    ss << "helper subprocess #" << assigned_id_;
  }
  return ss.str();
}

nacl::string NaClSubprocess::detailed_description() const {
  nacl::stringstream ss;
  ss << description()
     << "={ this=" << static_cast<const void*>(this)
     << ", srpc_client=" << static_cast<void*>(srpc_client_.get())
     << ", service_runtime=" << static_cast<void*>(service_runtime_.get())
     << " }";
  return ss.str();
}

// Shutdown the socket connection and service runtime, in that order.
void NaClSubprocess::Shutdown() {
  srpc_client_.reset(NULL);
  if (service_runtime_.get() != NULL) {
    service_runtime_->Shutdown();
    service_runtime_.reset(NULL);
  }
}

NaClSubprocess::~NaClSubprocess() {
  Shutdown();
}

bool NaClSubprocess::StartSrpcServices() {
  srpc_client_.reset(service_runtime_->SetupAppChannel());
  return NULL != srpc_client_.get();
}

bool NaClSubprocess::StartJSObjectProxy(Plugin* plugin, ErrorInfo* error_info) {
  return srpc_client_->StartJSObjectProxy(plugin, error_info);
}

bool NaClSubprocess::HasMethod(uintptr_t method_id) const {
  if (NULL == srpc_client_.get()) {
    return false;
  }
  return srpc_client_->HasMethod(method_id);
}

bool NaClSubprocess::InitParams(uintptr_t method_id, SrpcParams* params) const {
  if (NULL == srpc_client_.get()) {
    return false;
  }
  return srpc_client_->InitParams(method_id, params);
}

bool NaClSubprocess::Invoke(uintptr_t method_id, SrpcParams* params) const {
  if (NULL == srpc_client_.get()) {
    return false;
  }
  return srpc_client_->Invoke(method_id, params);
}

}  // namespace plugin
