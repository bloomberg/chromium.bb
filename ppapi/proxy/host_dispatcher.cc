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

typedef std::map<PP_Module, HostDispatcher*> ModuleToDispatcherMap;
ModuleToDispatcherMap* g_module_to_dispatcher = NULL;

PP_Bool ReserveInstanceID(PP_Module module, PP_Instance instance) {
  // Default to returning true (usable) failure. Otherwise, if there's some
  // kind of communication error or the plugin just crashed, we'll get into an
  // infinite loop generating new instnace IDs since we think they're all in
  // use.
  ModuleToDispatcherMap::const_iterator found =
      g_module_to_dispatcher->find(module);
  if (found == g_module_to_dispatcher->end()) {
    NOTREACHED();
    return PP_TRUE;
  }

  bool usable = true;
  if (!found->second->Send(new PpapiMsg_ReserveInstanceId(instance, &usable)))
    return PP_TRUE;
  return BoolToPPBool(usable);
}

// Saves the state of the given bool and puts it back when it goes out of
// scope.
class BoolRestorer {
 public:
  BoolRestorer(bool* var) : var_(var), old_value_(*var) {
  }
  ~BoolRestorer() {
    *var_ = old_value_;
  }
 private:
  bool* var_;
  bool old_value_;
};

}  // namespace

HostDispatcher::HostDispatcher(base::ProcessHandle remote_process_handle,
                               PP_Module module,
                               GetInterfaceFunc local_get_interface)
    : Dispatcher(remote_process_handle, local_get_interface),
      pp_module_(module),
      ppb_proxy_(NULL) {
  if (!g_module_to_dispatcher)
    g_module_to_dispatcher = new ModuleToDispatcherMap;
  (*g_module_to_dispatcher)[pp_module_] = this;

  const PPB_Var_Deprecated* var_interface =
      static_cast<const PPB_Var_Deprecated*>(
          local_get_interface(PPB_VAR_DEPRECATED_INTERFACE));
  SetSerializationRules(new HostVarSerializationRules(var_interface, module));

  memset(plugin_interface_support_, 0,
         sizeof(PluginInterfaceSupport) * INTERFACE_ID_COUNT);

  ppb_proxy_ = reinterpret_cast<const PPB_Proxy_Private*>(
      GetLocalInterface(PPB_PROXY_PRIVATE_INTERFACE));
  DCHECK(ppb_proxy_) << "The proxy interface should always be supported.";

  ppb_proxy_->SetReserveInstanceIDCallback(pp_module_, &ReserveInstanceID);
}

HostDispatcher::~HostDispatcher() {
  g_module_to_dispatcher->erase(pp_module_);
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

bool HostDispatcher::Send(IPC::Message* msg) {
  // Normal sync messages are set to unblock, which would normally cause the
  // plugin to be reentered to process them. We only want to do this when we
  // know the plugin is in a state to accept reentrancy. Since the plugin side
  // never clears this flag on messages it sends, we can't get deadlock, but we
  // may still get reentrancy in the host as a result.
  if (!allow_plugin_reentrancy_)
    msg->set_unblock(false);
  return Dispatcher::Send(msg);
}

bool HostDispatcher::OnMessageReceived(const IPC::Message& msg) {
  // We only want to allow reentrancy when the most recent message from the
  // plugin was a scripting message. We save the old state of the flag on the
  // stack in case we're (we are the host) being reentered ourselves. The flag
  // is set to false here for all messages, and then the scripting API will
  // explicitly set it to true during processing of those messages that can be
  // reentered.
  BoolRestorer restorer(&allow_plugin_reentrancy_);
  allow_plugin_reentrancy_ = false;

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
    proxy = CreatePPBInterfaceProxy(info);
  }

  return proxy->OnMessageReceived(msg);
}

void HostDispatcher::OnChannelError() {
  Dispatcher::OnChannelError();  // Stop using the channel.

  // Tell the host about the crash so it can clean up.
  ppb_proxy_->PluginCrashed(pp_module());
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

InterfaceProxy* HostDispatcher::GetOrCreatePPBInterfaceProxy(
    InterfaceID id) {
  InterfaceProxy* proxy = target_proxies_[id].get();
  if (!proxy) {
    const InterfaceProxy::Info* info = GetPPBInterfaceInfo(id);
    if (!info)
      return NULL;

    // Sanity check. This function won't normally be called for trusted
    // interfaces, but in case somebody does this, we don't want to then give
    // the plugin the ability to call that trusted interface (since the
    // checking occurs at proxy-creation time).
    if (info->is_trusted && disallow_trusted_interfaces())
      return NULL;

    proxy = CreatePPBInterfaceProxy(info);
  }
  return proxy;
}

InterfaceProxy* HostDispatcher::CreatePPBInterfaceProxy(
    const InterfaceProxy::Info* info) {
  const void* local_interface = GetLocalInterface(info->name);
  if (!local_interface) {
    // This should always succeed since the browser should support the stuff
    // the proxy does. If this happens, something is out of sync.
    NOTREACHED();
    return NULL;
  }

  InterfaceProxy* proxy = info->create_proxy(this, local_interface);
  target_proxies_[info->id].reset(proxy);
  return proxy;
}

}  // namespace proxy
}  // namespace pp

