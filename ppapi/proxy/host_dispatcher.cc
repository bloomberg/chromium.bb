// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_dispatcher.h"

#include <map>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/host_var_serialization_rules.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_creation_proxy.h"

namespace ppapi {
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
  return PP_FromBool(usable);
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
      ppb_proxy_(NULL),
      allow_plugin_reentrancy_(false) {
  if (!g_module_to_dispatcher)
    g_module_to_dispatcher = new ModuleToDispatcherMap;
  (*g_module_to_dispatcher)[pp_module_] = this;

  const PPB_Var* var_interface =
      static_cast<const PPB_Var*>(local_get_interface(PPB_VAR_INTERFACE));
  SetSerializationRules(new HostVarSerializationRules(var_interface, module));

  ppb_proxy_ = reinterpret_cast<const PPB_Proxy_Private*>(
      local_get_interface(PPB_PROXY_PRIVATE_INTERFACE));
  DCHECK(ppb_proxy_) << "The proxy interface should always be supported.";

  ppb_proxy_->SetReserveInstanceIDCallback(pp_module_, &ReserveInstanceID);
}

HostDispatcher::~HostDispatcher() {
  g_module_to_dispatcher->erase(pp_module_);
}

bool HostDispatcher::InitHostWithChannel(
    Delegate* delegate,
    const IPC::ChannelHandle& channel_handle,
    bool is_client,
    const ppapi::Preferences& preferences) {
  if (!Dispatcher::InitWithChannel(delegate, channel_handle, is_client))
    return false;
  Send(new PpapiMsg_SetPreferences(preferences));
  return true;
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
  TRACE_EVENT2("ppapi proxy", "HostDispatcher::Send",
               "Class", IPC_MESSAGE_ID_CLASS(msg->type()),
               "Line", IPC_MESSAGE_ID_LINE(msg->type()));
  // Prevent the dispatcher from going away during the call. Scenarios
  // where this could happen include a Send for a sync message which while
  // waiting for the reply, dispatches an incoming ExecuteScript call which
  // destroys the plugin module and in turn the dispatcher.
  ScopedModuleReference ref(this);

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
  TRACE_EVENT2("ppapi proxy", "HostDispatcher::OnMessageReceived",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  // We only want to allow reentrancy when the most recent message from the
  // plugin was a scripting message. We save the old state of the flag on the
  // stack in case we're (we are the host) being reentered ourselves. The flag
  // is set to false here for all messages, and then the scripting API will
  // explicitly set it to true during processing of those messages that can be
  // reentered.
  BoolRestorer restorer(&allow_plugin_reentrancy_);
  allow_plugin_reentrancy_ = false;

  return Dispatcher::OnMessageReceived(msg);
}

void HostDispatcher::OnChannelError() {
  Dispatcher::OnChannelError();  // Stop using the channel.

  // Tell the host about the crash so it can clean up and display notification.
  ppb_proxy_->PluginCrashed(pp_module());
}

const void* HostDispatcher::GetProxiedInterface(const std::string& iface_name) {
  const void* proxied_interface =
      InterfaceList::GetInstance()->GetInterfaceForPPP(iface_name);
  if (!proxied_interface)
    return NULL;  // Don't have a proxy for this interface, don't query further.

  PluginSupportedMap::iterator iter(plugin_supported_.find(iface_name));
  if (iter == plugin_supported_.end()) {
    // Need to query. Cache the result so we only do this once.
    bool supported = false;

    bool previous_reentrancy_value = allow_plugin_reentrancy_;
    allow_plugin_reentrancy_ = true;
    Send(new PpapiMsg_SupportsInterface(iface_name, &supported));
    allow_plugin_reentrancy_ = previous_reentrancy_value;

    std::pair<PluginSupportedMap::iterator, bool> iter_success_pair;
    iter_success_pair = plugin_supported_.insert(
        PluginSupportedMap::value_type(iface_name, supported));
    iter = iter_success_pair.first;
  }
  if (iter->second)
    return proxied_interface;
  return NULL;
}

void HostDispatcher::OnInvalidMessageReceived() {
  // TODO(brettw) bug 95345 kill the plugin when an invalid message is
  // received.
}

// ScopedModuleReference -------------------------------------------------------

ScopedModuleReference::ScopedModuleReference(Dispatcher* dispatcher) {
  DCHECK(!dispatcher->IsPlugin());
  dispatcher_ = static_cast<HostDispatcher*>(dispatcher);
  dispatcher_->ppb_proxy()->AddRefModule(dispatcher_->pp_module());
}

ScopedModuleReference::~ScopedModuleReference() {
  dispatcher_->ppb_proxy()->ReleaseModule(dispatcher_->pp_module());
}

}  // namespace proxy
}  // namespace ppapi
