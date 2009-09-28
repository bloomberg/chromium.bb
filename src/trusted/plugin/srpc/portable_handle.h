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


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PORTABLE_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PORTABLE_HANDLE_H_

// TODO(gregoryd): probably too many includes here.
#include <stdio.h>
#include <map>
#include <string>
#include "native_client/src/include/base/basictypes.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/method_map.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"


namespace nacl_srpc {

// Forward declarations for externals.
class Plugin;

struct PortableHandleInitializer {
  PortablePluginInterface* plugin_interface_;
  PortableHandleInitializer(PortablePluginInterface* plugin_interface):
      plugin_interface_(plugin_interface) {}
};


typedef enum {
  METHOD_CALL = 0,
  PROPERTY_GET,
  PROPERTY_SET
} CallType;


// PortableHandle is the struct used to represent handles that are opaque to
// the javascript bridge.
class PortableHandle {
 public:
  // There are derived classes, so the constructor and destructor must
  // be visible.
  explicit PortableHandle();
  virtual ~PortableHandle();

  bool Init(struct PortableHandleInitializer* init_info);
  PortablePluginInterface* GetPortablePluginInterface() {
    return plugin_interface_;
  }

  // generic NPAPI/IDispatch interface
  bool Invoke(uintptr_t method_id, CallType call_type, SrpcParams* params);
  bool HasMethod(uintptr_t method_id, CallType call_type);

  // get the method signature so ScritableHandle can marshal the inputs
  bool InitParams(uintptr_t method_id, CallType call_type, SrpcParams* params);
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);
  static int number_alive() { return number_alive_counter; }

 protected:
   // function_ptr must be static
  void AddMethodToMap(RpcFunction function_ptr,
                      const char* name,
                      CallType call_type,
                      const char* ins,
                      const char* outs);
  // Add a function that will be called through the default handler
  // Dynamic methods are called by index
  // that is provided as part of the MethodInfo
  void AddDynamicMethodToMap(MethodInfo* method_info);

  // Every derived class should provide an implementation
  // for the next two functions to allow handling of method calls
  // that cannot be registered at build time.
  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);
  virtual Plugin* GetPlugin() = 0;
private:
  MethodInfo* GetMethodInfo(uintptr_t method_id, CallType call_type);

 public:
  MethodMap methods_;
  MethodMap property_get_methods_;
  MethodMap property_set_methods_;

 private:
  static int number_alive_counter;
  PortablePluginInterface* plugin_interface_;
};

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PORTABLE_HANDLE_H_
