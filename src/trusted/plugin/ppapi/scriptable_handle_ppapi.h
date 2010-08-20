/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// PPAPI-based implementation of the interface for a scriptable object.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_

#include <string>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "ppapi/cpp/scriptable_object.h"
#include "ppapi/cpp/var.h"

namespace plugin {

class PortableHandle;

// Encapsulates a browser scriptable object for a PPAPI NaCl plugin.
class ScriptableHandlePpapi : public pp::ScriptableObject,
                              public ScriptableHandle {
 public:
  // Factory method for creation.
  static ScriptableHandlePpapi* New(PortableHandle* handle);

  // ------ Methods inherited from pp::ScriptableObject:

  // Returns true for preloaded NaCl Plugin properties.
  // Does not set |exception|.
  virtual bool HasProperty(const pp::Var& name, pp::Var* exception);
  // Returns true for preloaded NaCl Plugin methods and SRPC methods exported
  // from a NaCl module. Does not set |exception|.
  virtual bool HasMethod(const pp::Var& name, pp::Var* exception);

  // Gets the value of a preloaded NaCl Plugin property.
  // Sets |exception| on failure.
  virtual pp::Var GetProperty(const pp::Var& name, pp::Var* exception);
  // Sets the value of a preloaded NaCl Plugin property.
  // Does not add new properties. Sets |exception| of failure.
  virtual void SetProperty(const pp::Var& name, const pp::Var& value,
                           pp::Var* exception);
  // Set |exception| to indicate that property removal is not supported.
  virtual void RemoveProperty(const pp::Var& name, pp::Var* exception);
  // Returns a list of all preloaded NaCl Plugin |properties|.
  // Does not set |exception|.
  virtual void GetAllPropertyNames(std::vector<pp::Var>* properties,
                                   pp::Var* exception);

  // Calls preloaded NaCl Plugin methods or SRPC methods exported from
  // a NaCl module. Sets |exception| on failure.
  virtual pp::Var Call(const pp::Var& name, const std::vector<pp::Var>& args,
                       pp::Var* exception);

  // Sets |exception| to indicate that constructor is not supported.
  virtual pp::Var Construct(const std::vector<pp::Var>& args,
                            pp::Var* exception);

  // ----- Methods inherited from ScriptableHandle:

  // Reference counting is handled by pp::Var that wraps this ScriptableObject,
  // so there is nothing to implement here.
  virtual ScriptableHandle* AddRef() { return this; }
  virtual void Unref() { }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ScriptableHandlePpapi);
  // Prevent construction from outside the class:
  // must use factory New() method instead.
  explicit ScriptableHandlePpapi(PortableHandle* handle);
  virtual ~ScriptableHandlePpapi();

  // Helper functionality common to HasProperty and HasMethod.
  bool HasCallType(CallType call_type, nacl::string call_name,
                   const char* caller);
  // Helper functionality common to GetProperty, SetProperty and Call.
  // If |call_type| is PROPERTY_GET, ignores args and expects 1 return var.
  // If |call_type| is PROPERTY_SET, expects 1 arg and returns void var.
  // Sets |exception| on failure.
  pp::Var Invoke(CallType call_type, nacl::string call_name, const char* caller,
                 const std::vector<pp::Var>& args, pp::Var* exception);
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_
