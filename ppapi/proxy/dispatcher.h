// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DISPATCHER_H_
#define PPAPI_PROXY_DISPATCHER_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/tracked_objects.h"
#include "ipc/ipc_channel_proxy.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/callback_tracker.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_var_tracker.h"

namespace ppapi {
class WebKitForwarding;
}

namespace pp {
namespace proxy {

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
class Dispatcher : public ProxyChannel {
 public:
  typedef const void* (*GetInterfaceFunc)(const char*);
  typedef int32_t (*InitModuleFunc)(PP_Module, GetInterfaceFunc);

  class Delegate : public ProxyChannel::Delegate {
   public:
    // Returns the set used for globally uniquifying PP_Instances. This same
    // set must be returned for all channels. This is required only for the
    // plugin side, for the host side, the return value may be NULL.
    //
    // DEREFERENCE ONLY ON THE I/O THREAD.
    virtual std::set<PP_Instance>* GetGloballySeenInstanceIDSet() = 0;

    // Returns the WebKit forwarding object used to make calls into WebKit.
    // Necessary only on the plugin side. The host side can return NULL.
    virtual ppapi::WebKitForwarding* GetWebKitForwarding() = 0;

    // Posts the given task to the WebKit thread associated with this plugin
    // process. For host processes, this will not be called and can do
    // nothing. The WebKit thread should be lazily created if it does not
    // exist yet.
    virtual void PostToWebKitThread(const tracked_objects::Location& from_here,
                                    const base::Closure& task) = 0;
  };

  virtual ~Dispatcher();

  // Returns true if the dispatcher is on the plugin side, or false if it's the
  // browser side.
  virtual bool IsPlugin() const = 0;

  VarSerializationRules* serialization_rules() const {
    return serialization_rules_.get();
  }

  // Wrapper for calling the local GetInterface function.
  const void* GetLocalInterface(const char* interface_name);

  // Returns the pointer to the IO thread for processing IPC messages.
  // TODO(brettw) remove this. It's a hack to support the Flash
  // ModuleLocalThreadAdapter. When the thread stuff is sorted out, this
  // implementation detail should be hidden.
  base::MessageLoopProxy* GetIPCMessageLoop();

  // Adds the given filter to the IO thread. Takes ownership of the pointer.
  // TODO(brettw) remove this. It's a hack to support the Flash
  // ModuleLocalThreadAdapter. When the thread stuff is sorted out, this
  // implementation detail should be hidden.
  void AddIOThreadMessageFilter(IPC::ChannelProxy::MessageFilter* filter);

  // TODO(brettw): What is this comment referring to?
  // Called if the remote side is declaring to us which interfaces it supports
  // so we don't have to query for each one. We'll pre-create proxies for
  // each of the given interfaces.

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  CallbackTracker& callback_tracker() {
    return callback_tracker_;
  }

  // Retrieves the information associated with the given interface, identified
  // either by name or ID. Each function searches either PPP or PPB interfaces.
  static const InterfaceProxy::Info* GetPPBInterfaceInfo(
      const std::string& name);
  static const InterfaceProxy::Info* GetPPBInterfaceInfo(
      InterfaceID id);
  static const InterfaceProxy::Info* GetPPPInterfaceInfo(
      const std::string& name);
  static const InterfaceProxy::Info* GetPPPInterfaceInfo(
      InterfaceID id);

 protected:
  Dispatcher(base::ProcessHandle remote_process_handle,
             GetInterfaceFunc local_get_interface);

  void SetDelegate(Delegate* delegate);

  // Setter for the derived classes to set the appropriate var serialization.
  // Takes ownership of the given pointer, which must be on the heap.
  void SetSerializationRules(VarSerializationRules* var_serialization_rules);

  bool disallow_trusted_interfaces() const {
    return disallow_trusted_interfaces_;
  }

  Delegate* dispatcher_delegate_;

 private:
  bool disallow_trusted_interfaces_;

  GetInterfaceFunc local_get_interface_;

  CallbackTracker callback_tracker_;

  scoped_ptr<VarSerializationRules> serialization_rules_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_DISPATCHER_H_
