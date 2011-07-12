// Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
#include "native_client/src/trusted/plugin/portable_handle.h"

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
                 ScriptableHandle* socket)
    : assigned_id_(assigned_id),
      service_runtime_(service_runtime),
      socket_(socket) {
  }
  virtual ~NaClSubprocess();

  ServiceRuntime* service_runtime() const { return service_runtime_; }
  void set_service_runtime(ServiceRuntime* service_runtime) {
    service_runtime_ = service_runtime;
  }

  // The socket used for communicating w/ the NaCl module.
  ScriptableHandle* socket() const { return socket_; }
  void set_socket(ScriptableHandle* socket) { socket_ = socket; }

  // A basic description of the subprocess.
  nacl::string description();

  // A detailed description of the subprocess that may contain addresses.
  // Only use for debugging, but do not expose this to untrusted webapps.
  nacl::string detailed_description();

  // Start up interfaces.
  bool StartSrpcServices();
  bool StartJSObjectProxy(Plugin* plugin, ErrorInfo* error_info);

  // Interact.
  bool HasMethod(uintptr_t method_id, CallType call_type) const;
  bool InitParams(uintptr_t method_id,
                  CallType call_type,
                  SrpcParams* params) const;
  bool Invoke(uintptr_t method_id,
              CallType call_type,
              SrpcParams* params) const;

  // Fully shut down the subprocess.
  void Shutdown();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(NaClSubprocess);

  NaClSubprocessId assigned_id_;

  // The service runtime representing the NaCl module instance.
  ServiceRuntime* service_runtime_;

  // Ownership of socket taken from the service runtime.
  ScriptableHandle* socket_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_NACL_SUBPROCESS_H_
