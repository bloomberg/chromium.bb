/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// The abstract portable scriptable object base class.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PORTABLE_HANDLE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PORTABLE_HANDLE_H_

#include <stdio.h>
#include <map>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/utility.h"


struct NaClDesc;

namespace nacl {
class DescWrapper;
}  // namespace

namespace plugin {

// Forward declarations for externals.
class BrowserInterface;
class ErrorInfo;
class Plugin;
class ScriptableHandle;
class ServiceRuntime;

typedef enum {
  METHOD_CALL = 0,
  PROPERTY_GET,
  PROPERTY_SET
} CallType;


// PortableHandle represents scriptable objects used by the browser plugin.
// The classes in this hierarchy are independent of the browser plugin API
// used to implement them.  PortableHandle is an abstract base class.
class PortableHandle {
 public:
  // Delete this object.
  void Delete() { delete this; }

  // A call to Invalidate() means the object should discard any
  // refcounted objects it holds, without using Unref(), e.g. by
  // setting the pointers to NULL.  These objects have either already
  // been deallocated or will be deallocated shortly.
  virtual void Invalidate() = 0;

  // Generic dispatch interface
  bool Invoke(uintptr_t method_id, CallType call_type, SrpcParams* params);
  bool HasMethod(uintptr_t method_id, CallType call_type);

  std::vector<uintptr_t>* GetPropertyIdentifiers() {
    return property_get_methods_.Keys();
  }

  // Get the method signature so ScriptableHandle can marshal the inputs
  bool InitParams(uintptr_t method_id, CallType call_type, SrpcParams* params);

  // The interface to the browser.
  virtual BrowserInterface* browser_interface() const = 0;

  // Every portable object has a pointer to the root plugin object.
  virtual Plugin* plugin() const = 0;

  // DescBasedHandle objects encapsulate a descriptor.
  virtual nacl::DescWrapper* wrapper() const { return NULL; }
  virtual NaClDesc* desc() const { return NULL; }

  // This is only implemented for ConnectedSocket, to avoid an unchecked
  // downcast.  It reports an error if this binding is invoked.
  virtual bool StartJSObjectProxy(Plugin* plugin, ErrorInfo* error_info) {
    UNREFERENCED_PARAMETER(plugin);
    UNREFERENCED_PARAMETER(error_info);
    NACL_NOTREACHED();
    return false;
  }

 protected:
  PortableHandle();
  virtual ~PortableHandle();

  // Derived classes can set the properties and methods they export by
  // the following three methods.
  void AddPropertyGet(RpcFunction function_ptr,
                      const char* name,
                      const char* outs);
  void AddPropertySet(RpcFunction function_ptr,
                      const char* name,
                      const char* ins);
  void AddMethodCall(RpcFunction function_ptr,
                     const char* name,
                     const char* ins,
                     const char* outs);

  // Every derived class should provide an implementation for these functions
  // to allow handling of method calls that cannot be registered at build time.
  virtual bool InitParamsEx(uintptr_t method_id,
                            CallType call_type,
                            SrpcParams* params);
  virtual bool InvokeEx(uintptr_t method_id,
                        CallType call_type,
                        SrpcParams* params);
  virtual bool HasMethodEx(uintptr_t method_id, CallType call_type);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PortableHandle);
  MethodInfo* GetMethodInfo(uintptr_t method_id, CallType call_type);
  MethodMap methods_;
  MethodMap property_get_methods_;
  MethodMap property_set_methods_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PORTABLE_HANDLE_H_
