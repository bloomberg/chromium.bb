// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Instances of NaCl modules spun up within the plugin as a subprocess.
// This may represent the "main" nacl module, or it may represent helpers
// that perform various tasks within the plugin, for example,
// a NaCl module for a compiler could be loaded to translate LLVM bitcode
// into native code.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/service_runtime.h"
#include "native_client/src/trusted/plugin/srpc_client.h"

namespace plugin {

class Plugin;
class ServiceRuntime;

// Identifier for helper NaCl nexes. Should be non-negative for valid nexes.
typedef int32_t NaClSubprocessId;
const NaClSubprocessId kInvalidNaClSubprocessId = -1;
const NaClSubprocessId kMainSubprocessId = -2;

// A class representing an instance of a NaCl module, loaded by the plugin.
class NaClSubprocess {
 public:
  NaClSubprocess(NaClSubprocessId assigned_id,
                 ServiceRuntime* service_runtime,
                 SrpcClient* srpc_client)
    : assigned_id_(assigned_id),
      service_runtime_(service_runtime),
      srpc_client_(srpc_client) {
  }
  virtual ~NaClSubprocess();

  ServiceRuntime* service_runtime() const { return service_runtime_.get(); }
  void set_service_runtime(ServiceRuntime* service_runtime) {
    service_runtime_.reset(service_runtime);
  }

  // The socket used for communicating w/ the NaCl module.
  SrpcClient* srpc_client() const { return srpc_client_.get(); }
  void set_socket(SrpcClient* srpc_client) { srpc_client_.reset(srpc_client); }

  // A basic description of the subprocess.
  nacl::string description() const;

  // A detailed description of the subprocess that may contain addresses.
  // Only use for debugging, but do not expose this to untrusted webapps.
  nacl::string detailed_description() const;

  // Start up interfaces.
  bool StartSrpcServices();
  bool StartJSObjectProxy(Plugin* plugin, ErrorInfo* error_info);

  // Interact.
  bool HasMethod(uintptr_t method_id) const;
  bool InitParams(uintptr_t method_id, SrpcParams* params) const;
  bool Invoke(uintptr_t method_id, SrpcParams* params) const;

  // Fully shut down the subprocess.
  void Shutdown();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NaClSubprocess);

  NaClSubprocessId assigned_id_;

  // The service runtime representing the NaCl module instance.
  nacl::scoped_ptr<ServiceRuntime> service_runtime_;

  // Ownership of srpc_client taken from the service runtime.
  nacl::scoped_ptr<SrpcClient> srpc_client_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_
