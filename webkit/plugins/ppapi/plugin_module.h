// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PLUGIN_MODULE_H_
#define WEBKIT_PLUGINS_PPAPI_PLUGIN_MODULE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/native_library.h"
#include "base/process.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/webkit_plugins_export.h"

class FilePath;
typedef void* NPIdentifier;

namespace ppapi {
class CallbackTracker;
class WebKitForwarding;
}  // namespace ppapi

namespace WebKit {
class WebPluginContainer;
}  // namespace WebKit

namespace webkit {
namespace ppapi {

class PluginDelegate;
class PluginInstance;

// Represents one plugin library loaded into one renderer. This library may
// have multiple instances.
//
// Note: to get from a PP_Instance to a PluginInstance*, use the
// ResourceTracker.
class WEBKIT_PLUGINS_EXPORT PluginModule :
    public base::RefCounted<PluginModule>,
    public base::SupportsWeakPtr<PluginModule> {
 public:
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int (*PPP_InitializeModuleFunc)(PP_Module, PPB_GetInterface);
  typedef void (*PPP_ShutdownModuleFunc)();

  struct EntryPoints {
    // This structure is POD, with the constructor initializing to NULL.
    WEBKIT_PLUGINS_EXPORT EntryPoints();

    GetInterfaceFunc get_interface;
    PPP_InitializeModuleFunc initialize_module;
    PPP_ShutdownModuleFunc shutdown_module;  // Optional, may be NULL.
  };

  // Allows the embedder to associate a class with this module. This is opaque
  // from the PluginModule's perspective (see Set/GetEmbedderState below) but
  // the module is in charge of deleting the class.
  class EmbedderState {
   public:
    virtual ~EmbedderState() {}
  };

  typedef std::set<PluginInstance*> PluginInstanceSet;

  // You must call one of the Init functions after the constructor to create a
  // module of the type you desire.
  //
  // The module lifetime delegate is a non-owning pointer that must outlive
  // all plugin modules. In practice it will be a global singleton that
  // tracks which modules are alive.
  PluginModule(const std::string& name,
               const FilePath& path,
               PluginDelegate::ModuleLifetime* lifetime_delegate,
               const ::ppapi::PpapiPermissions& perms);

  ~PluginModule();

  // Sets the given class as being associated with this module. It will be
  // deleted when the module is destroyed. You can only set it once, subsequent
  // sets will assert.
  //
  // See EmbedderState above for more.
  void SetEmbedderState(scoped_ptr<EmbedderState> state);
  EmbedderState* GetEmbedderState();

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

  // Creates a new module for a NaCl instance that will be using the IPC proxy.
  // We can't use the existing module, or new instances of the plugin can't
  // be created.
  scoped_refptr<PluginModule> CreateModuleForNaClInstance();

  // Initializes the NaCl module for the out of process proxy. InitAsProxied
  // must be called before calling InitAsProxiedNaCl. Returns a NaCl result code
  // indicating whether the proxy started successfully or there was an error.
  PP_NaClResult InitAsProxiedNaCl(PluginInstance* instance);

  bool IsProxied() const;

  // Returns the peer process ID if the plugin is running out of process;
  // returns |base::kNullProcessId| otherwise.
  base::ProcessId GetPeerProcessId();

  static const PPB_Core* GetCore();

  // Returns a pointer to the local GetInterface function for retrieving
  // PPB interfaces.
  static GetInterfaceFunc GetLocalGetInterfaceFunc();

  // Returns whether an interface is supported. This method can be called from
  // the browser process and used for interface matching before plugin
  // registration.
  // NOTE: those custom interfaces provided by PpapiInterfaceFactoryManager
  // will not be considered when called on the browser process.
  static bool SupportsInterface(const char* name);

  // Returns the module handle. This may be used before Init() is called (the
  // proxy needs this information to set itself up properly).
  PP_Module pp_module() const { return pp_module_; }

  const std::string& name() const { return name_; }
  const FilePath& path() const { return path_; }
  const ::ppapi::PpapiPermissions& permissions() const { return permissions_; }

  PluginInstance* CreateInstance(PluginDelegate* delegate,
                                 WebKit::WebPluginContainer* container,
                                 const GURL& plugin_url);

  // Returns "some" plugin instance associated with this module. This is not
  // guaranteed to be any one in particular. This is normally used to execute
  // callbacks up to the browser layer that are not inherently per-instance,
  // but the delegate lives only on the plugin instance so we need one of them.
  PluginInstance* GetSomeInstance() const;

  const PluginInstanceSet& GetAllInstances() const { return instances_; }

  // Calls the plugin's GetInterface and returns the given interface pointer,
  // which could be NULL.
  const void* GetPluginInterface(const char* name) const;

  // This module is associated with a set of instances. The PluginInstance
  // object declares its association with this module in its destructor and
  // releases us in its destructor.
  void InstanceCreated(PluginInstance* instance);
  void InstanceDeleted(PluginInstance* instance);

  scoped_refptr< ::ppapi::CallbackTracker> GetCallbackTracker();

  // Called when running out of process and the plugin crashed. This will
  // release relevant resources and update all affected instances.
  void PluginCrashed();

  bool is_in_destructor() const { return is_in_destructor_; }
  bool is_crashed() const { return is_crashed_; }

  // Reserves the given instance is unique within the plugin, checking for
  // collisions. See PPB_Proxy_Private for more information.
  //
  // The setter will set the callback which is set up when the proxy
  // initializes. The Reserve function will call the previously set callback if
  // it exists to validate the ID. If the callback has not been set (such as
  // for in-process plugins), the Reserve function will assume that the ID is
  // usable and will return true.
  void SetReserveInstanceIDCallback(
      PP_Bool (*reserve)(PP_Module, PP_Instance));
  bool ReserveInstanceID(PP_Instance instance);

  // These should only be called from the main thread.
  void SetBroker(PluginDelegate::Broker* broker);
  PluginDelegate::Broker* GetBroker();

 private:
  // Calls the InitializeModule entrypoint. The entrypoint must have been
  // set and the plugin must not be out of process (we don't maintain
  // entrypoints in that case).
  bool InitializeModule(const EntryPoints& entry_points);

  // Note: This may be null.
  PluginDelegate::ModuleLifetime* lifetime_delegate_;

  // See EmbedderState above.
  scoped_ptr<EmbedderState> embedder_state_;

  // Tracker for completion callbacks, used mainly to ensure that all callbacks
  // are properly aborted on module shutdown.
  scoped_refptr< ::ppapi::CallbackTracker> callback_tracker_;

  PP_Module pp_module_;

  // True when we're running in the destructor. This allows us to write some
  // assertions.
  bool is_in_destructor_;

  // True if the plugin is running out-of-process and has crashed.
  bool is_crashed_;

  // Manages the out of process proxy interface. The presence of this
  // pointer indicates that the plugin is running out of process and that the
  // entry_points_ aren't valid.
  scoped_ptr<PluginDelegate::OutOfProcessProxy> out_of_process_proxy_;

  // Non-owning pointer to the broker for this plugin module, if one exists.
  // It is populated and cleared in the main thread.
  PluginDelegate::Broker* broker_;

  // Holds a reference to the base::NativeLibrary handle if this PluginModule
  // instance wraps functions loaded from a library.  Can be NULL.  If
  // |library_| is non-NULL, PluginModule will attempt to unload the library
  // during destruction.
  base::NativeLibrary library_;

  // Contains pointers to the entry points of the actual plugin implementation.
  // These will be NULL for out-of-process plugins, which is indicated by the
  // presence of the out_of_process_proxy_ value.
  EntryPoints entry_points_;

  // The name and file location of the module.
  const std::string name_;
  const FilePath path_;

  ::ppapi::PpapiPermissions permissions_;

  // Non-owning pointers to all instances associated with this module. When
  // there are no more instances, this object should be deleted.
  PluginInstanceSet instances_;

  PP_Bool (*reserve_instance_id_)(PP_Module, PP_Instance);

  DISALLOW_COPY_AND_ASSIGN(PluginModule);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PLUGIN_MODULE_H_
