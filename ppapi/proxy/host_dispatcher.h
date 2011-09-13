// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_DISPATCHER_H_
#define PPAPI_PROXY_HOST_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/process.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/shared_impl/function_group_base.h"

struct PPB_Proxy_Private;
struct PPB_Var_Deprecated;

namespace base {
class WaitableEvent;
}

namespace IPC {
class SyncChannel;
}

namespace ppapi {

struct Preferences;

namespace proxy {

class VarSerialization;

class PPAPI_PROXY_EXPORT HostDispatcher : public Dispatcher {
 public:
  // Constructor for the renderer side.
  //
  // You must call InitHostWithChannel after the constructor.
  HostDispatcher(base::ProcessHandle host_process_handle,
                 PP_Module module,
                 GetInterfaceFunc local_get_interface);
  ~HostDispatcher();

  // You must call this function before anything else. Returns true on success.
  // The delegate pointer must outlive this class, ownership is not
  // transferred.
  virtual bool InitHostWithChannel(Delegate* delegate,
                                   const IPC::ChannelHandle& channel_handle,
                                   bool is_client,
                                   const Preferences& preferences);

  // The host side maintains a mapping from PP_Instance to Dispatcher so
  // that we can send the messages to the right channel.
  static HostDispatcher* GetForInstance(PP_Instance instance);
  static void SetForInstance(PP_Instance instance,
                             HostDispatcher* dispatcher);
  static void RemoveForInstance(PP_Instance instance);

  // Returns the host's notion of our PP_Module. This will be different than
  // the plugin's notion of its PP_Module because the plugin process may be
  // used by multiple renderer processes.
  //
  // Use this value instead of a value from the plugin whenever talking to the
  // host.
  PP_Module pp_module() const { return pp_module_; }

  // Dispatcher overrides.
  virtual bool IsPlugin() const;
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // Proxied version of calling GetInterface on the plugin. This will check
  // if the plugin supports the given interface (with caching) and returns the
  // pointer to the proxied interface if it is supported. Returns NULL if the
  // given interface isn't supported by the plugin or the proxy.
  const void* GetProxiedInterface(const std::string& iface_name);

  // See the value below. Call this when processing a scripting message from
  // the plugin that can be reentered. This is set to false at the beginning
  // of processing of each message from the plugin.
  void set_allow_plugin_reentrancy() {
    allow_plugin_reentrancy_ = true;
  }

  // Returns the proxy interface for talking to the implementation.
  const PPB_Proxy_Private* ppb_proxy() const { return ppb_proxy_; }

 protected:
  // Overridden from Dispatcher.
  virtual void OnInvalidMessageReceived();

 private:
  PP_Module pp_module_;

  // Maps interface name to whether that interface is supported. If an interface
  // name is not in the map, that implies that we haven't queried for it yet.
  typedef base::hash_map<std::string, bool> PluginSupportedMap;
  PluginSupportedMap plugin_supported_;

  // Guaranteed non-NULL.
  const PPB_Proxy_Private* ppb_proxy_;

  // Set to true when the plugin is in a state that it can be reentered by a
  // sync message from the host. We allow reentrancy only when we're processing
  // a sync message from the renderer that is a scripting command. When the
  // plugin is in this state, it needs to accept reentrancy since scripting may
  // ultimately call back into the plugin.
  bool allow_plugin_reentrancy_;

  DISALLOW_COPY_AND_ASSIGN(HostDispatcher);
};

// Create this object on the stack to prevent the module (and hence the
// dispatcher) from being deleted out from under you. This is necessary when
// calling some scripting functions that may delete the plugin.
//
// This may only be called in the host. The parameter is a plain Dispatcher
// since that's what most callers have.
class ScopedModuleReference {
 public:
  ScopedModuleReference(Dispatcher* dispatcher);
  ~ScopedModuleReference();

 private:
  HostDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ScopedModuleReference);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_HOST_DISPATCHER_H_
