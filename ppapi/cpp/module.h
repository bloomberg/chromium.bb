// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MODULE_H_
#define PPAPI_CPP_MODULE_H_

#include <map>
#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/cpp/core.h"


/// @file
/// This file defines a Module class.
namespace pp {

class Instance;

/// The Module class.  The browser calls CreateInstance() to create
/// an instance of your module on the web page.  The browser creates a new
/// instance for each <code>\<embed></code> tag with
/// <code>type="application/x-nacl"</code>
class Module {
 public:
  typedef std::map<PP_Instance, Instance*> InstanceMap;

  // You may not call any other PP functions from the constructor, put them
  // in Init instead. Various things will not be set up until the constructor
  // completes.
  Module();
  virtual ~Module();

  // Returns the global instance of this module object, or NULL if the module
  // is not initialized yet.
  static Module* Get();

  // This function will be automatically called after the object is created.
  // This is where you can put functions that rely on other parts of the API,
  // now that the module has been created.
  virtual bool Init();

  // Returns the internal module handle.
  PP_Module pp_module() const { return pp_module_; }

  // Returns the internal get_browser_interface pointer.
  // TODO(sehr): This should be removed once the NaCl browser plugin no longer
  // needs it.
  PPB_GetInterface get_browser_interface() const {
    return get_browser_interface_;
  }

  // Returns the core interface for doing basic global operations. This is
  // guaranteed to be non-NULL once the module has successfully initialized
  // and during the Init() call.
  //
  // It will be NULL before Init() has been called.
  Core* core() { return core_; }

  // Implements GetInterface for the browser to get plugin interfaces. If you
  // need to provide your own implementations of new interfaces, you can use
  // AddPluginInterface which this function will use.
  const void* GetPluginInterface(const char* interface_name);

  // Returns an interface in the browser.
  const void* GetBrowserInterface(const char* interface_name);

  // Returns the object associated with this PP_Instance, or NULL if one is
  // not found.
  Instance* InstanceForPPInstance(PP_Instance instance);

  // Adds a handler for a given interface name. When the browser requests
  // that interface name, the given |vtable| will be returned.
  //
  // In general, plugins will not need to call this directly. Instead, the
  // C++ wrappers for each interface will register themselves with this
  // function.
  //
  // This function may be called more than once with the same interface name
  // and vtable with no effect. However, it may not be used to register a
  // different vtable for an already-registered interface. It will assert for
  // a different registration for an already-registered interface in debug
  // mode, and just ignore the registration in release mode.
  void AddPluginInterface(const std::string& interface_name,
                          const void* vtable);

  // Sets the browser interface and calls the regular init function that
  // can be overridden by the base classes.
  //
  // TODO(brettw) make this private when I can figure out how to make the
  // initialize function a friend.
  bool InternalInit(PP_Module mod,
                    PPB_GetInterface get_browser_interface);

  // Allows iteration over the current instances in the module.
  const InstanceMap& current_instances() const { return current_instances_; }

 protected:
  // Override to create your own plugin type.
  virtual Instance* CreateInstance(PP_Instance instance) = 0;

 private:
  friend PP_Bool Instance_DidCreate(PP_Instance pp_instance,
                                    uint32_t argc,
                                    const char* argn[],
                                    const char* argv[]);
  friend void Instance_DidDestroy(PP_Instance instance);

  // Unimplemented (disallow copy and assign).
  Module(const Module&);
  Module& operator=(const Module&);

  // Instance tracking.
  InstanceMap current_instances_;

  PP_Module pp_module_;
  PPB_GetInterface get_browser_interface_;

  Core* core_;

  // All additional interfaces this plugin can handle as registered by
  // AddPluginInterface.
  typedef std::map<std::string, const void*> InterfaceMap;
  InterfaceMap additional_interfaces_;
};

}  // namespace pp

#endif  // PPAPI_CPP_MODULE_H_
