// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_dispatcher.h"

#include <map>

#include "base/logging.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/proxy/host_var_serialization_rules.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

typedef std::map<PP_Instance, HostDispatcher*> InstanceToDispatcherMap;
InstanceToDispatcherMap* g_instance_to_dispatcher = NULL;

}  // namespace

HostDispatcher::HostDispatcher(base::ProcessHandle remote_process_handle,
                               PP_Module module,
                               GetInterfaceFunc local_get_interface)
    : Dispatcher(remote_process_handle, local_get_interface),
      pp_module_(module),
      ppb_proxy_(NULL) {
  const PPB_Var_Deprecated* var_interface =
      static_cast<const PPB_Var_Deprecated*>(
          local_get_interface(PPB_VAR_DEPRECATED_INTERFACE));
  SetSerializationRules(new HostVarSerializationRules(var_interface, module));

  memset(plugin_interface_support_, 0,
         sizeof(PluginInterfaceSupport) * INTERFACE_ID_COUNT);
}

HostDispatcher::~HostDispatcher() {
}

// static
HostDispatcher* HostDispatcher::GetForInstance(PP_Instance instance) {
  if (!g_instance_to_dispatcher)
    return NULL;
  InstanceToDispatcherMap::iterator found = g_instance_to_dispatcher->find(
      instance);
  if (found == g_instance_to_dispatcher->end())
    return NULL;
  return found->second;
}

// static
void HostDispatcher::SetForInstance(PP_Instance instance,
                                    HostDispatcher* dispatcher) {
  if (!g_instance_to_dispatcher)
    g_instance_to_dispatcher = new InstanceToDispatcherMap;
  (*g_instance_to_dispatcher)[instance] = dispatcher;
}

// static
void HostDispatcher::RemoveForInstance(PP_Instance instance) {
  if (!g_instance_to_dispatcher)
    return;
  InstanceToDispatcherMap::iterator found = g_instance_to_dispatcher->find(
      instance);
  if (found != g_instance_to_dispatcher->end())
    g_instance_to_dispatcher->erase(found);
}

bool HostDispatcher::IsPlugin() const {
  return false;
}

bool HostDispatcher::OnMessageReceived(const IPC::Message& msg) {
  // Handle common control messages.
  if (Dispatcher::OnMessageReceived(msg))
    return true;

  if (msg.routing_id() <= 0 && msg.routing_id() >= INTERFACE_ID_COUNT) {
    NOTREACHED();
    // TODO(brettw): kill the plugin if it starts sending invalid messages?
    return true;
  }

  InterfaceProxy* proxy = target_proxies_[msg.routing_id()].get();
  if (!proxy) {
    // Autocreate any proxy objects to handle requests from the plugin. Since
    // we always support all known PPB_* interfaces (modulo the trusted bit),
    // there's very little checking necessary.
    const InterfaceProxy::Info* info = GetPPBInterfaceInfo(
        static_cast<InterfaceID>(msg.routing_id()));
    if (!info ||
        (info->is_trusted && disallow_trusted_interfaces()))
      return true;

    const void* local_interface = GetLocalInterface(info->name);
    if (!local_interface) {
      // This should always succeed since the browser should support the stuff
      // the proxy does. If this happens, something is out of sync.
      NOTREACHED();
      return true;
    }

    proxy = info->create_proxy(this, local_interface);
    target_proxies_[info->id].reset(proxy);
  }

  return proxy->OnMessageReceived(msg);
}

void HostDispatcher::OnChannelError() {
  Dispatcher::OnChannelError();  // Stop using the channel.

  // Tell the host about the crash so it can clean up.
  GetPPBProxy()->PluginCrashed(pp_module());
}

const void* HostDispatcher::GetProxiedInterface(const std::string& interface) {
  // First see if we even have a proxy for this interface.
  const InterfaceProxy::Info* info = GetPPPInterfaceInfo(interface);
  if (!info)
    return NULL;

  if (plugin_interface_support_[static_cast<int>(info->id)] !=
      INTERFACE_UNQUERIED) {
    // Already queried the plugin if it supports this interface.
    if (plugin_interface_support_[info->id] == INTERFACE_SUPPORTED)
      return info->interface;
    return NULL;
  }

  // Need to re-query. Cache the result so we only do this once.
  bool supported = false;
  Send(new PpapiMsg_SupportsInterface(interface, &supported));
  plugin_interface_support_[static_cast<int>(info->id)] =
      supported ? INTERFACE_SUPPORTED : INTERFACE_UNSUPPORTED;

  if (supported)
    return info->interface;
  return NULL;
}

const PPB_Proxy_Private* HostDispatcher::GetPPBProxy() {
  if (!ppb_proxy_) {
    ppb_proxy_ = reinterpret_cast<const PPB_Proxy_Private*>(
        GetLocalInterface(PPB_PROXY_PRIVATE_INTERFACE));
  }
  return ppb_proxy_;
}

}  // namespace proxy
}  // namespace pp

