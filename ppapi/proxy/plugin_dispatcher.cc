// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_dispatcher.h"

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_var_serialization_rules.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppp_class_proxy.h"

#if defined(OS_POSIX)
#include "base/eintr_wrapper.h"
#include "ipc/ipc_channel_posix.h"
#endif

namespace pp {
namespace proxy {

namespace {

typedef std::map<PP_Instance, PluginDispatcher*> InstanceToDispatcherMap;
InstanceToDispatcherMap* g_instance_to_dispatcher = NULL;

}  // namespace

PluginDispatcher::PluginDispatcher(base::ProcessHandle remote_process_handle,
                                   GetInterfaceFunc get_interface)
    : Dispatcher(remote_process_handle, get_interface)
#if defined(OS_POSIX)
    , renderer_fd_(-1)
#endif
    {
  SetSerializationRules(new PluginVarSerializationRules);

  // As a plugin, we always support the PPP_Class interface. There's no
  // GetInterface call or name for it, so we insert it into our table now.
  target_proxies_[INTERFACE_ID_PPP_CLASS].reset(new PPP_Class_Proxy(this));
}

PluginDispatcher::~PluginDispatcher() {
#if defined(OS_POSIX)
  CloseRendererFD();
#endif
}

// static
PluginDispatcher* PluginDispatcher::GetForInstance(PP_Instance instance) {
  if (!g_instance_to_dispatcher)
    return NULL;
  InstanceToDispatcherMap::iterator found = g_instance_to_dispatcher->find(
      instance);
  if (found == g_instance_to_dispatcher->end())
    return NULL;
  return found->second;
}

// static
const void* PluginDispatcher::GetInterfaceFromDispatcher(
    const char* interface) {
  // All interfaces the plugin requests of the browser are "PPB".
  const InterfaceProxy::Info* info = GetPPBInterfaceInfo(interface);
  if (!info)
    return NULL;
  return info->interface;
}

bool PluginDispatcher::InitWithChannel(
    Delegate* delegate,
    const IPC::ChannelHandle& channel_handle,
    bool is_client) {
  if (!Dispatcher::InitWithChannel(delegate, channel_handle, is_client))
    return false;

  // The message filter will intercept and process certain messages directly
  // on the I/O thread.
  channel()->AddFilter(
      new PluginMessageFilter(delegate->GetGloballySeenInstanceIDSet()));
  return true;
}

bool PluginDispatcher::IsPlugin() const {
  return true;
}

bool PluginDispatcher::OnMessageReceived(const IPC::Message& msg) {
  // Handle common control messages.
  if (Dispatcher::OnMessageReceived(msg))
    return true;

  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Handle some plugin-specific control messages.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PluginDispatcher, msg)
      IPC_MESSAGE_HANDLER(PpapiMsg_SupportsInterface, OnMsgSupportsInterface)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  if (msg.routing_id() <= 0 && msg.routing_id() >= INTERFACE_ID_COUNT) {
    // Host is sending us garbage. Since it's supposed to be trusted, this
    // isn't supposed to happen. Crash here in all builds in case the renderer
    // is compromised.
    CHECK(false);
    return true;
  }

  // There are two cases:
  //
  // * The first case is that the host is calling a PPP interface. It will
  //   always do a check for the interface before sending messages, and this
  //   will create the necessary interface proxy at that time. So when we
  //   actually receive a message, we know such a proxy will exist.
  //
  // * The second case is that the host is sending a response to the plugin
  //   side of a PPB interface (some, like the URL loader, have complex
  //   response messages). Since the host is trusted and not supposed to be
  //   doing silly things, we can just create a PPB proxy project on demand the
  //   first time it's needed.

  InterfaceProxy* proxy = target_proxies_[msg.routing_id()].get();
  if (!proxy) {
    // Handle the first time the host calls a PPB reply interface by
    // autocreating it.
    const InterfaceProxy::Info* info = GetPPBInterfaceInfo(
        static_cast<InterfaceID>(msg.routing_id()));
    if (!info) {
      NOTREACHED();
      return true;
    }
    proxy = info->create_proxy(this, NULL);
    target_proxies_[info->id].reset(proxy);
  }

  return proxy->OnMessageReceived(msg);
}

void PluginDispatcher::OnChannelError() {
  Dispatcher::OnChannelError();

  // The renderer has crashed. This channel and all instances associated with
  // it are no longer valid.
  ForceFreeAllInstances();
  // TODO(brettw) free resources too!
  delete this;
}

void PluginDispatcher::DidCreateInstance(PP_Instance instance) {
  if (!g_instance_to_dispatcher)
    g_instance_to_dispatcher = new InstanceToDispatcherMap;
  (*g_instance_to_dispatcher)[instance] = this;

  instance_map_[instance] = InstanceData();
}

void PluginDispatcher::DidDestroyInstance(PP_Instance instance) {
  InstanceDataMap::iterator it = instance_map_.find(instance);
  if (it != instance_map_.end())
    instance_map_.erase(it);

  if (g_instance_to_dispatcher) {
    InstanceToDispatcherMap::iterator found = g_instance_to_dispatcher->find(
        instance);
    if (found != g_instance_to_dispatcher->end()) {
      DCHECK(found->second == this);
      g_instance_to_dispatcher->erase(found);
    } else {
      NOTREACHED();
    }
  }
}

InstanceData* PluginDispatcher::GetInstanceData(PP_Instance instance) {
  InstanceDataMap::iterator it = instance_map_.find(instance);
  return (it == instance_map_.end()) ? NULL : &it->second;
}

#if defined(OS_POSIX)
int PluginDispatcher::GetRendererFD() {
  if (renderer_fd_ == -1 && channel())
    renderer_fd_ = channel()->GetClientFileDescriptor();
  return renderer_fd_;
}

void PluginDispatcher::CloseRendererFD() {
  if (renderer_fd_ != -1) {
    if (HANDLE_EINTR(close(renderer_fd_)) < 0)
      PLOG(ERROR) << "close";
    renderer_fd_ = -1;
  }
}
#endif

void PluginDispatcher::ForceFreeAllInstances() {
  if (!g_instance_to_dispatcher)
    return;

  // Iterating will remove each item from the map, so we need to make a copy
  // to avoid things changing out from under is.
  InstanceToDispatcherMap temp_map = *g_instance_to_dispatcher;
  for (InstanceToDispatcherMap::iterator i = temp_map.begin();
       i != temp_map.end(); ++i) {
    if (i->second == this) {
      // Synthesize an "instance destroyed" message, this will notify the
      // plugin and also remove it from our list of tracked plugins.
      OnMessageReceived(
          PpapiMsg_PPPInstance_DidDestroy(INTERFACE_ID_PPP_INSTANCE, i->first));
    }
  }
}

void PluginDispatcher::OnMsgSupportsInterface(
    const std::string& interface_name,
    bool* result) {
  *result = false;

  // Setup a proxy for receiving the messages from this interface.
  const InterfaceProxy::Info* info = GetPPPInterfaceInfo(interface_name);
  if (!info)
    return;  // Interface not supported by proxy.

  // Check for a cached result.
  if (target_proxies_[info->id].get()) {
    *result = true;
    return;
  }

  // Query the plugin & cache the result.
  const void* interface_functions = GetLocalInterface(interface_name.c_str());
  if (!interface_functions)
    return;
  target_proxies_[info->id].reset(
      info->create_proxy(this, interface_functions));
  *result = true;
}

}  // namespace proxy
}  // namespace pp

