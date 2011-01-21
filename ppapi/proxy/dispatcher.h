// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DISPATCHER_H_
#define PPAPI_PROXY_DISPATCHER_H_

#include <map>
#include <string>
#include <vector>

#include "base/linked_ptr.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/callback_tracker.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/plugin_var_tracker.h"

class MessageLoop;
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
class VarSerializationRules;

// An interface proxy can represent either end of a cross-process interface
// call. The "source" side is where the call is invoked, and the "target" side
// is where the call ends up being executed.
//
// Plugin side                          | Browser side
// -------------------------------------|--------------------------------------
//                                      |
//    "Source"                          |    "Target"
//    InterfaceProxy ----------------------> InterfaceProxy
//                                      |
//                                      |
//    "Target"                          |    "Source"
//    InterfaceProxy <---------------------- InterfaceProxy
//                                      |
class Dispatcher : public IPC::Channel::Listener,
                   public IPC::Message::Sender {
 public:
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int32_t (*InitModuleFunc)(PP_Module, GetInterfaceFunc);
  typedef void (*ShutdownModuleFunc)();

  ~Dispatcher();

  bool InitWithChannel(MessageLoop* ipc_message_loop,
                       const IPC::ChannelHandle& channel_handle,
                       bool is_client,
                       base::WaitableEvent* shutdown_event);

  // Returns true if the dispatcher is on the plugin side, or false if it's the
  // browser side.
  virtual bool IsPlugin() const = 0;

  VarSerializationRules* serialization_rules() const {
    return serialization_rules_.get();
  }
  PP_Module pp_module() const {
    return pp_module_;
  }

  // Wrapper for calling the local GetInterface function.
  const void* GetLocalInterface(const char* interface);

  // Implements PPP_GetInterface and PPB_GetInterface on the "source" side. It
  // will check if the remote side supports this interface as a target, and
  // create a proxy if it does. A local implementation of that interface backed
  // by the proxy will be returned on success. If the interface is unproxyable
  // or not supported by the remote side, returns NULL.
  const void* GetProxiedInterface(const std::string& interface);

  // Returns the remote process' handle. For the host dispatcher, this will be
  // the plugin process, and for the plugin dispatcher, this will be the
  // renderer process. This is used for sharing memory and such and is
  // guaranteed valid (unless the remote process has suddenly died).
  base::ProcessHandle remote_process_handle() const {
    return remote_process_handle_;
  }

  // Called if the remote side is declaring to us which interfaces it supports
  // so we don't have to query for each one. We'll pre-create proxies for
  // each of the given interfaces.

  // IPC::Message::Sender implementation.
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  IPC::SyncChannel* channel() const {
    return channel_.get();
  }

  CallbackTracker& callback_tracker() {
    return callback_tracker_;
  }

 protected:
  Dispatcher(base::ProcessHandle remote_process_handle,
             GetInterfaceFunc local_get_interface);

  // Setter for the derived classes to set the appropriate var serialization.
  // Takes ownership of the given pointer, which must be on the heap.
  void SetSerializationRules(VarSerializationRules* var_serialization_rules);

  void set_pp_module(PP_Module module) {
    pp_module_ = module;
  }

  // Allows the PluginDispatcher to add a magic proxy for PPP_Class, bypassing
  // the normal "do you support this proxy" stuff and the big lookup of
  // name to proxy object. Takes ownership of the pointer.
  void InjectProxy(InterfaceID id,
                   const std::string& name,
                   InterfaceProxy* proxy);

 private:
  typedef std::map< std::string, linked_ptr<InterfaceProxy> > ProxyMap;

  // Message handlers
  void OnMsgSupportsInterface(const std::string& interface_name, bool* result);
  void OnMsgDeclareInterfaces(const std::vector<std::string>& interfaces);

  // Allocates a new proxy on the heap corresponding to the given interface
  // name, or returns NULL if that interface name isn't known proxyable. The
  // caller owns the returned pointer.
  //
  // The interface_functions gives the pointer to the local interfece when this
  // is a target proxy. When creating a source proxy, set this to NULL.
  InterfaceProxy* CreateProxyForInterface(
      const std::string& interface_name,
      const void* interface_functions);

  // Returns true if the remote side supports the given interface as the
  // target of an IPC call.
  bool RemoteSupportsTargetInterface(const std::string& interface);

  // Sets up a proxy as the target for the given interface, if it is supported.
  // Returns true if this process implements the given interface and it is
  // proxyable.
  bool SetupProxyForTargetInterface(const std::string& interface);

  bool IsInterfaceTrusted(const std::string& interface);

  // Set by the derived classed to indicate the module ID corresponding to
  // this dispatcher.
  PP_Module pp_module_;

  base::ProcessHandle remote_process_handle_;  // See getter above.
  scoped_ptr<IPC::SyncChannel> channel_;

  bool disallow_trusted_interfaces_;

  GetInterfaceFunc local_get_interface_;

  ProxyMap proxies_;
  InterfaceProxy* id_to_proxy_[INTERFACE_ID_COUNT];

  // True if the remote side has declared which interfaces it supports in
  // advance. When set, it means if we don't already have a source proxy for
  // the requested interface, that the remote side doesn't support it and
  // we don't need to query.
  //
  // This is just an optimization. The browser has a fixed set of interfaces
  // it supports, and the many plugins will end up querying many of them. By
  // having the browser just send all of those interfaces in one message, we
  // can avoid a bunch of IPC chatter to set up each interface.
  bool declared_supported_remote_interfaces_;

  CallbackTracker callback_tracker_;

  scoped_ptr<VarSerializationRules> serialization_rules_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_DISPATCHER_H_
