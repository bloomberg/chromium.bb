// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PPAPI-based implementation of the interface for a scriptable object.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_

#include <string>
#include <vector>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"
#include "native_client/src/third_party/ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "native_client/src/third_party/ppapi/cpp/private/var_private.h"

namespace plugin {

class PortableHandle;

// Encapsulates a browser scriptable object for a PPAPI NaCl plugin.
class ScriptableHandlePpapi : public pp::deprecated::ScriptableObject,
                              public ScriptableHandle {
 public:
  // Factory method for creation.
  static ScriptableHandlePpapi* New(PortableHandle* handle);

  // If not NULL, this var should be reused to pass this object to the browser.
  pp::VarPrivate* var() { return var_; }

  // ------ Methods inherited from pp::deprecated::ScriptableObject:

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

  // This function is called when we are about to share the object owned by the
  // plugin with the browser. Since reference counting on the browser side is
  // handled via pp::Var's, we create the var() here if not created already.
  virtual ScriptableHandle* AddRef();
  // If var() is set, we delete it. Otherwise, we delete the object itself.
  // Therefore, this cannot be called more than once.
  virtual void Unref();

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ScriptableHandlePpapi);
  // Prevent construction from outside the class:
  // must use factory New() method instead.
  explicit ScriptableHandlePpapi(PortableHandle* handle);
  // This will be called when both the plugin and the browser clear all
  // references to this object.
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

  // When we pass the object owned by the plugin to the browser, we need to wrap
  // it in a pp::VarPrivate, which also registers the object with the browser
  // for refcounting. It must be registered only once with all other var
  // references being copies of the original one. Thus, we record the
  // pp::VarPrivate here and reuse it when satisfiying additional browser
  // requests. This way we also ensure that when the browser clears its
  // references, this object does not get deallocated while we still hold ours.
  // This is never set for objects that are not shared with the browser nor for
  // objects created during SRPC calls as they are taken over by the browser on
  // return.
  pp::VarPrivate* var_;

  // We should have no more than one internal plugin owner for this object,
  // and only that owner should call Unref(). To CHECK for that keep a counter.
  int num_unref_calls_;

  bool handle_is_plugin_;  // Whether (portable) handle() is a plugin.
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_SCRIPTABLE_HANDLE_PPAPI_H_
