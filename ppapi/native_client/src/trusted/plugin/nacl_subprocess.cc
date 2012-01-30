// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/nacl_subprocess.h"

#include <stdarg.h>

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/plugin_error.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/trusted/plugin/service_runtime.h"

namespace plugin {

nacl::string NaClSubprocess::description() const {
  return description_;
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

bool NaClSubprocess::InvokeSrpcMethod(const nacl::string& method_name,
                                      const nacl::string& input_signature,
                                      SrpcParams* params,
                                      ...) {
  va_list vl;
  va_start(vl, params);
  bool result = VInvokeSrpcMethod(method_name, input_signature, params, vl);
  va_end(vl);
  return result;
}

bool NaClSubprocess::VInvokeSrpcMethod(const nacl::string& method_name,
                                       const nacl::string& input_signature,
                                       SrpcParams* params,
                                       va_list vl) {
  uintptr_t method_ident;
  if (!SetupSrpcInvocation(method_name, params, &method_ident)) {
    return false;
  }

  // Set up inputs.
  for (size_t i = 0; i < input_signature.length(); ++i) {
    char c = input_signature[i];
    // Only handle the limited number of SRPC types used for PNaCl.
    // Add more as needed.
    switch (c) {
      default:
        PLUGIN_PRINTF(("PnaclSrpcLib::InvokeSrpcMethod unhandled type: %c\n",
                       c));
        return false;
      case NACL_SRPC_ARG_TYPE_BOOL: {
        int input = va_arg(vl, int);
        params->ins()[i]->u.bval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_DOUBLE: {
        double input = va_arg(vl, double);
        params->ins()[i]->u.dval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY: {
        // SrpcParam's destructor *should* free the dup'ed string.
        const char* orig_str = va_arg(vl, const char*);
        char* input = strdup(orig_str);
        params->ins()[i]->arrays.str = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_HANDLE: {
        NaClSrpcImcDescType input = va_arg(vl, NaClSrpcImcDescType);
        params->ins()[i]->u.hval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_INT: {
        int32_t input = va_arg(vl, int32_t);
        params->ins()[i]->u.ival = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_LONG: {
        int64_t input = va_arg(vl, int64_t);
        params->ins()[i]->u.lval = input;
        break;
      }
    }
  }

  return Invoke(method_ident, params);
}

bool NaClSubprocess::SetupSrpcInvocation(const nacl::string& method_name,
                                         SrpcParams* params,
                                         uintptr_t* method_ident) {
  *method_ident = browser_interface_->StringToIdentifier(method_name);
  if (!HasMethod(*method_ident)) {
    PLUGIN_PRINTF(("SetupSrpcInvocation (no %s method found)\n",
                   method_name.c_str()));
    return false;
  }
  return InitParams(*method_ident, params);
}

}  // namespace plugin
