// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "build/build_config.h"
#include "ppapi/c/dev/ppb_console_dev.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppb_view_shared.h"

namespace ppapi {

struct Preferences;
class Resource;

namespace thunk {
class PPB_Instance_API;
class ResourceCreationAPI;
}

namespace proxy {

// Used to keep track of per-instance data.
struct InstanceData {
  InstanceData();
  ~InstanceData();

  ViewData view;

  PP_Bool flash_fullscreen;  // Used for PPB_FlashFullscreen.

  // When non-0, indicates the callback to execute when mouse lock is lost.
  PP_CompletionCallback mouse_lock_callback;
};

class PPAPI_PROXY_EXPORT PluginDispatcher
    : public Dispatcher,
      public base::SupportsWeakPtr<PluginDispatcher> {
 public:
  class PPAPI_PROXY_EXPORT PluginDelegate : public ProxyChannel::Delegate {
   public:
    // Returns the set used for globally uniquifying PP_Instances. This same
    // set must be returned for all channels.
    //
    // DEREFERENCE ONLY ON THE I/O THREAD.
    virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() = 0;

    // Registers the plugin dispatcher and returns an ID.
    // Plugin dispatcher IDs will be used to dispatch messages from the browser.
    // Each call to Register() has to be matched with a call to Unregister().
    virtual uint32 Register(PluginDispatcher* plugin_dispatcher) = 0;
    virtual void Unregister(uint32 plugin_dispatcher_id) = 0;
  };

  // Constructor for the plugin side. The init and shutdown functions will be
  // will be automatically called when requested by the renderer side. The
  // module ID will be set upon receipt of the InitializeModule message.
  //
  // You must call InitPluginWithChannel after the constructor.
  PluginDispatcher(PP_GetInterface_Func get_interface,
                   bool incognito);
  virtual ~PluginDispatcher();

  // The plugin side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel if there are multiple
  // renderers sharing the same plugin. This mapping is maintained by
  // DidCreateInstance/DidDestroyInstance.
  static PluginDispatcher* GetForInstance(PP_Instance instance);

  // Same as GetForInstance but retrieves the instance from the given resource
  // object as a convenience. Returns NULL on failure.
  static PluginDispatcher* GetForResource(const Resource* resource);

  // Implements the GetInterface function for the plugin to call to retrieve
  // a browser interface.
  static const void* GetBrowserInterface(const char* interface_name);

  // Logs the given log message to the given instance, or, if the instance is
  // invalid, to all instances associated with all dispatchers. Used for
  // global log messages.
  static void LogWithSource(PP_Instance instance,
                            PP_LogLevel_Dev level,
                            const std::string& source,
                            const std::string& value);

  const void* GetPluginInterface(const std::string& interface_name);

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  bool InitPluginWithChannel(PluginDelegate* delegate,
                             const IPC::ChannelHandle& channel_handle,
                             bool is_client);

  // Dispatcher overrides.
  virtual bool IsPlugin() const;
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Keeps track of which dispatcher to use for each instance, active instances
  // and tracks associated data like the current size.
  void DidCreateInstance(PP_Instance instance);
  void DidDestroyInstance(PP_Instance instance);

  // Gets the data for an existing instance, or NULL if the instance id doesn't
  // correspond to a known instance.
  InstanceData* GetInstanceData(PP_Instance instance);

  // Returns the corresponding API. These are APIs not associated with a
  // resource. Guaranteed non-NULL.
  thunk::PPB_Instance_API* GetInstanceAPI();
  thunk::ResourceCreationAPI* GetResourceCreationAPI();

  // Returns the Preferences.
  const Preferences& preferences() const { return preferences_; }

  uint32 plugin_dispatcher_id() const { return plugin_dispatcher_id_; }
  bool incognito() const { return incognito_; }

 private:
  friend class PluginDispatcherTest;

  // Notifies all live instances that they're now closed. This is used when
  // a renderer crashes or some other error is received.
  void ForceFreeAllInstances();

  // IPC message handlers.
  void OnMsgSupportsInterface(const std::string& interface_name, bool* result);
  void OnMsgSetPreferences(const Preferences& prefs);

  PluginDelegate* plugin_delegate_;

  // Contains all the plugin interfaces we've queried. The mapped value will
  // be the pointer to the interface pointer supplied by the plugin if it's
  // supported, or NULL if it's not supported. This allows us to cache failures
  // and not req-query if a plugin doesn't support the interface.
  typedef base::hash_map<std::string, const void*> InterfaceMap;
  InterfaceMap plugin_interfaces_;

  typedef base::hash_map<PP_Instance, InstanceData> InstanceDataMap;
  InstanceDataMap instance_map_;

  // The preferences sent from the host. We only want to set this once, which
  // is what the received_preferences_ indicates. See OnMsgSetPreferences.
  bool received_preferences_;
  Preferences preferences_;

  uint32 plugin_dispatcher_id_;

  // Set to true when the instances associated with this dispatcher are
  // incognito mode.
  bool incognito_;

  DISALLOW_COPY_AND_ASSIGN(PluginDispatcher);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
