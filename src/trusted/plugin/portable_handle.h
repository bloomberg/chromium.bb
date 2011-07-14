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

  // Get the method signature so ScriptableHandle can marshal the inputs
  virtual bool InitParams(uintptr_t method_id,
                          CallType call_type,
                          SrpcParams* params) {
    UNREFERENCED_PARAMETER(method_id);
    UNREFERENCED_PARAMETER(call_type);
    UNREFERENCED_PARAMETER(params);
    return false;
  }

  // Generic dispatch interface
  virtual bool Invoke(uintptr_t method_id,
                      CallType call_type,
                      SrpcParams* params) {
    UNREFERENCED_PARAMETER(method_id);
    UNREFERENCED_PARAMETER(call_type);
    UNREFERENCED_PARAMETER(params);
    return false;
  }
  virtual bool HasMethod(uintptr_t method_id, CallType call_type) {
    UNREFERENCED_PARAMETER(method_id);
    UNREFERENCED_PARAMETER(call_type);
    return false;
  }

  virtual std::vector<uintptr_t>* GetPropertyIdentifiers() {
    return NULL;
  }

  // The interface to the browser.
  virtual BrowserInterface* browser_interface() const = 0;

  // Every portable object has a pointer to the root plugin object.
  virtual Plugin* plugin() const = 0;

  // DescBasedHandle objects encapsulate a descriptor.
  virtual NaClDesc* desc() const { return NULL; }

 protected:
  PortableHandle();
  virtual ~PortableHandle();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PortableHandle);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PORTABLE_HANDLE_H_
