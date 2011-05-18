// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_DISPATCHER_H_
#define PPAPI_PROXY_PLUGIN_DISPATCHER_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "build/build_config.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/shared_impl/function_group_base.h"

class MessageLoop;

namespace base {
class WaitableEvent;
}

namespace pp {
namespace proxy {

// Used to keep track of per-instance data.
struct InstanceData {
  InstanceData() : fullscreen(PP_FALSE) {}
  PP_Rect position;
  PP_Bool fullscreen;
};

class PluginDispatcher : public Dispatcher {
 public:
  class PluginDelegate : public ProxyChannel::Delegate {
   public:
    // Returns the set used for globally uniquifying PP_Instances. This same
    // set must be returned for all channels.
    //
    // DEREFERENCE ONLY ON THE I/O THREAD.
    virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() = 0;

    // Returns the WebKit forwarding object used to make calls into WebKit.
    // Necessary only on the plugin side.
    virtual ppapi::WebKitForwarding* GetWebKitForwarding() = 0;

    // Posts the given task to the WebKit thread associated with this plugin
    // process. The WebKit thread should be lazily created if it does not
    // exist yet.
    virtual void PostToWebKitThread(const tracked_objects::Location& from_here,
                                    const base::Closure& task) = 0;

    // Sends the given message to the browser. Identical semantics to
    // IPC::Message::Sender interface.
    virtual bool SendToBrowser(IPC::Message* msg) = 0;
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

  static const void* GetInterfaceFromDispatcher(const char* interface);

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
  ppapi::WebKitForwarding* GetWebKitForwarding();

  // Returns the "new-style" function API for the given interface ID, creating
  // it if necessary.
  // TODO(brettw) this is in progress. It should be merged with the target
  // proxies so there is one list to consult.
  ppapi::FunctionGroupBase* GetFunctionAPI(
      pp::proxy::InterfaceID id);

 private:
  friend class PluginDispatcherTest;

  // Notifies all live instances that they're now closed. This is used when
  // a renderer crashes or some other error is received.
  void ForceFreeAllInstances();

  // IPC message handlers.
  void OnMsgSupportsInterface(const std::string& interface_name, bool* result);

  PluginDelegate* plugin_delegate_;

  // All target proxies currently created. These are ones that receive
  // messages.
  scoped_ptr<InterfaceProxy> target_proxies_[INTERFACE_ID_COUNT];

  // Function proxies created for "new-style" FunctionGroups.
  // TODO(brettw) this is in progress. It should be merged with the target
  // proxies so there is one list to consult.
  scoped_ptr< ::ppapi::FunctionGroupBase >
      function_proxies_[INTERFACE_ID_COUNT];

  typedef base::hash_map<PP_Instance, InstanceData> InstanceDataMap;
  InstanceDataMap instance_map_;

  DISALLOW_COPY_AND_ASSIGN(PluginDispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_DISPATCHER_H_
