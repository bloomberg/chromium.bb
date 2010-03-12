/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NPAPI Simple RPC Interface

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PORTABLE_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_PORTABLE_HANDLE_H_

// TODO(gregoryd): probably too many includes here.
#include <stdio.h>
#include <map>
#include "base/basictypes.h"
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
