// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PLUGIN_MODULE_H_
#define WEBKIT_PLUGINS_PPAPI_PLUGIN_MODULE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/native_library.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

class FilePath;
class MessageLoop;
struct PPB_Core;
typedef void* NPIdentifier;

namespace base {
class WaitableEvent;
}

namespace pp {
namespace proxy {
class HostDispatcher;
}  // proxy
}  // pp

namespace IPC {
struct ChannelHandle;
}

namespace webkit {
namespace ppapi {

class CallbackTracker;
class PluginDelegate;
class PluginInstance;

// Represents one plugin library loaded into one renderer. This library may
// have multiple instances.
//
// Note: to get from a PP_Instance to a PluginInstance*, use the
// ResourceTracker.
class PluginModule : public base::RefCounted<PluginModule>,
                     public base::SupportsWeakPtr<PluginModule> {
 public:
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int (*PPP_InitializeModuleFunc)(PP_Module, PPB_GetInterface);
  typedef void (*PPP_ShutdownModuleFunc)();

  struct EntryPoints {
    // This structure is POD, with the constructor initializing to NULL.
    EntryPoints();

    GetInterfaceFunc get_interface;
    PPP_InitializeModuleFunc initialize_module;
    PPP_ShutdownModuleFunc shutdown_module;  // Optional, may be NULL.
  };

  // You must call one of the Init functions after the constructor to create a
  // module of the type you desire.
  //
  // The module lifetime delegate is a non-owning pointer that must outlive
  // all plugin modules. In practice it will be a global singleton that
  // tracks which modules are alive.
  PluginModule(const std::string& name,
               PluginDelegate::ModuleLifetime* lifetime_delegate);

  ~PluginModule();

  // Initializes this module as an internal plugin with the given entrypoints.
  // This is used for "plugins" compiled into Chrome. Returns true on success.
  // False means that the plugin can not be used.
  bool InitAsInternalPlugin(const EntryPoints& entry_points);

  // Initializes this module using the given library path as the plugin.
  // Returns true on success. False means that the plugin can not be used.
  bool InitAsLibrary(const FilePath& path);

  // Initializes this module for the given out of process proxy. This takes
  // ownership of the given pointer, even in the failure case.
  void InitAsProxied(PluginDelegate::OutOfProcessProxy* out_of_process_proxy);

  static const PPB_Core* GetCore();

  // Returns a pointer to the local GetInterface function for retrieving
  // PPB interfaces.
  static GetInterfaceFunc GetLocalGetInterfaceFunc();

  // Returns the module handle. This may be used before Init() is called (the
  // proxy needs this information to set itself up properly).
  PP_Module pp_module() const { return pp_module_; }

  const std::string& name() const { return name_; }

  PluginInstance* CreateInstance(PluginDelegate* delegate);

  // Returns "some" plugin instance associated with this module. This is not
  // guaranteed to be any one in particular. This is normally used to execute
  // callbacks up to the browser layer that are not inherently per-instance,
  // but the delegate lives only on the plugin instance so we need one of them.
  PluginInstance* GetSomeInstance() const;

  // Calls the plugin's GetInterface and returns the given interface pointer,
  // which could be NULL.
  const void* GetPluginInterface(const char* name) const;

  // This module is associated with a set of instances. The PluginInstance
  // object declares its association with this module in its destructor and
  // releases us in its destructor.
  void InstanceCreated(PluginInstance* instance);
  void InstanceDeleted(PluginInstance* instance);

  scoped_refptr<CallbackTracker> GetCallbackTracker();

  // Called when running out of process and the plugin crashed. This will
  // release relevant resources and update all affected instances.
  void PluginCrashed();

 private:
  // Calls the InitializeModule entrypoint. The entrypoint must have been
  // set and the plugin must not be out of process (we don't maintain
  // entrypoints in that case).
  bool InitializeModule();

  PluginDelegate::ModuleLifetime* lifetime_delegate_;

  // Tracker for completion callbacks, used mainly to ensure that all callbacks
  // are properly aborted on module shutdown.
  scoped_refptr<CallbackTracker> callback_tracker_;

  PP_Module pp_module_;

  // Manages the out of process proxy interface. The presence of this
  // pointer indicates that the plugin is running out of process and that the
  // entry_points_ aren't valid.
  scoped_ptr<PluginDelegate::OutOfProcessProxy> out_of_process_proxy_;

  // Holds a reference to the base::NativeLibrary handle if this PluginModule
  // instance wraps functions loaded from a library.  Can be NULL.  If
  // |library_| is non-NULL, PluginModule will attempt to unload the library
  // during destruction.
  base::NativeLibrary library_;

  // Contains pointers to the entry points of the actual plugin implementation.
  // These will be NULL for out-of-process plugins, which is indicated by the
  // presence of the out_of_process_proxy_ value.
  EntryPoints entry_points_;

  // The name of the module.
  const std::string name_;

  // Non-owning pointers to all instances associated with this module. When
  // there are no more instances, this object should be deleted.
  typedef std::set<PluginInstance*> PluginInstanceSet;
  PluginInstanceSet instances_;

  DISALLOW_COPY_AND_ASSIGN(PluginModule);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PLUGIN_MODULE_H_
