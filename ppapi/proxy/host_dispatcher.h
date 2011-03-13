// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_HOST_DISPATCHER_H_
#define PPAPI_PROXY_HOST_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/process.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/plugin_var_tracker.h"

struct PPB_Proxy_Private;
struct PPB_Var_Deprecated;

namespace base {
class WaitableEvent;
}

namespace IPC {
class SyncChannel;
}

namespace pp {
namespace proxy {

class InterfaceProxy;
class VarSerialization;

class HostDispatcher : public Dispatcher {
 public:
  // Constructor for the renderer side.
  //
  // You must call Dispatcher::InitWithChannel after the constructor.
  HostDispatcher(base::ProcessHandle host_process_handle,
                 PP_Module module,
                 GetInterfaceFunc local_get_interface);
  ~HostDispatcher();

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
  const void* GetProxiedInterface(const std::string& interface);

  // Returns the proxy object associated with the given interface ID, creating
  // it if necessary. This is used in cases where a proxy needs to access code
  // in the proxy for another interface. It's assumed that the interface always
  // exists, so this is only used for browser proxies.
  //
  // Will return NULL if an interface isn't supported.
  InterfaceProxy* GetOrCreatePPBInterfaceProxy(InterfaceID id);

  // See the value below. Call this when processing a scripting message from
  // the plugin that can be reentered.
  void set_allow_plugin_reentrancy() {
    allow_plugin_reentrancy_ = true;
  }

  // Returns the proxy interface for talking to the implementation.
  const PPB_Proxy_Private* ppb_proxy() const { return ppb_proxy_; }

 private:
  friend class HostDispatcherTest;

  // Makes an instance of the given PPB interface proxy, storing it in the
  // target_proxies_ array. An proxy for this interface must not exist yet.
  InterfaceProxy* CreatePPBInterfaceProxy(const InterfaceProxy::Info* info);

  PP_Module pp_module_;

  enum PluginInterfaceSupport {
    INTERFACE_UNQUERIED = 0,  // Must be 0 so memset(0) will clear the list.
    INTERFACE_SUPPORTED,
    INTERFACE_UNSUPPORTED
  };
  PluginInterfaceSupport plugin_interface_support_[INTERFACE_ID_COUNT];

  // All target proxies currently created. These are ones that receive
  // messages. They are created on demand when we receive messages.
  scoped_ptr<InterfaceProxy> target_proxies_[INTERFACE_ID_COUNT];

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

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_HOST_DISPATCHER_H_
