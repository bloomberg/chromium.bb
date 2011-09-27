// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "build/build_config.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/ppapi_preferences.h"

class MessageLoop;

namespace base {
class WaitableEvent;
}

namespace ppapi {

struct Preferences;
class Resource;

namespace proxy {

// Used to keep track of per-instance data.
struct InstanceData {
  InstanceData();
  PP_Rect position;
  PP_Bool fullscreen;  // Used for PPB_Fullscreen_Dev.
  PP_Bool flash_fullscreen;  // Used for PPB_FlashFullscreen.
};

class PPAPI_PROXY_EXPORT PluginDispatcher : public Dispatcher {
 public:
  class PPAPI_PROXY_EXPORT PluginDelegate : public ProxyChannel::Delegate {
   public:
    // Returns the set used for globally uniquifying PP_Instances. This same
    // set must be returned for all channels.
    //
    // DEREFERENCE ONLY ON THE I/O THREAD.
    virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() = 0;

    // Returns the WebKit forwarding object used to make calls into WebKit.
    // Necessary only on the plugin side.
    virtual WebKitForwarding* GetWebKitForwarding() = 0;

    // Posts the given task to the WebKit thread associated with this plugin
    // process. The WebKit thread should be lazily created if it does not
    // exist yet.
    virtual void PostToWebKitThread(const tracked_objects::Location& from_here,
                                    const base::Closure& task) = 0;

    // Sends the given message to the browser. Identical semantics to
    // IPC::Message::Sender interface.
    virtual bool SendToBrowser(IPC::Message* msg) = 0;

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
  PluginDispatcher(base::ProcessHandle remote_process_handle,
                   GetInterfaceFunc get_interface);
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

  // Posts the given task to the WebKit thread.
  void PostToWebKitThread(const tracked_objects::Location& from_here,
                          const base::Closure& task);

  // Calls the PluginDelegate.SendToBrowser function.
  bool SendToBrowser(IPC::Message* msg);

  // Returns the WebKitForwarding object used to forward events to WebKit.
  WebKitForwarding* GetWebKitForwarding();

  // Returns the Preferences.
  const Preferences& preferences() const { return preferences_; }

  // Returns the "new-style" function API for the given interface ID, creating
  // it if necessary.
  // TODO(brettw) this is in progress. It should be merged with the target
  // proxies so there is one list to consult.
  FunctionGroupBase* GetFunctionAPI(InterfaceID id);

  uint32 plugin_dispatcher_id() const { return plugin_dispatcher_id_; }

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

  DISALLOW_COPY_AND_ASSIGN(PluginDispatcher);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
