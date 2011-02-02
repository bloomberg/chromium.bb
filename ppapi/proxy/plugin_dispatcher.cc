// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "ppapi/proxy/plugin_var_serialization_rules.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppp_class_proxy.h"

namespace pp {
namespace proxy {

namespace {

PluginDispatcher* g_dispatcher = NULL;

const void* GetInterfaceFromDispatcher(const char* interface) {
  // TODO(brettw) need some kind of lock for multi-thread access.
  return pp::proxy::PluginDispatcher::Get()->GetProxiedInterface(interface);
}

}  // namespace

PluginDispatcher::PluginDispatcher(base::ProcessHandle remote_process_handle,
                                   GetInterfaceFunc get_interface,
                                   InitModuleFunc init_module,
                                   ShutdownModuleFunc shutdown_module)
    : Dispatcher(remote_process_handle, get_interface),
      init_module_(init_module),
      shutdown_module_(shutdown_module) {
  SetSerializationRules(new PluginVarSerializationRules);

  // As a plugin, we always support the PPP_Class interface. There's no
  // GetInterface call or name for it, so we insert it into our table now.
  InjectProxy(INTERFACE_ID_PPP_CLASS, "$Internal_PPP_Class",
              new PPP_Class_Proxy(this));
}

PluginDispatcher::~PluginDispatcher() {
  if (shutdown_module_)
    shutdown_module_();
}

// static
PluginDispatcher* PluginDispatcher::Get() {
  return g_dispatcher;
}

// static
void PluginDispatcher::SetGlobal(PluginDispatcher* dispatcher) {
  DCHECK(!dispatcher || !g_dispatcher);
  g_dispatcher = dispatcher;
}

// static
PluginDispatcher* PluginDispatcher::GetForInstance(PP_Instance instance) {
  // TODO(brettw) implement "real" per-instance dispatcher map.
  DCHECK(instance != 0);
  return Get();
}

bool PluginDispatcher::IsPlugin() const {
  return true;
}

bool PluginDispatcher::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Handle some plugin-specific control messages.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PluginDispatcher, msg)
      IPC_MESSAGE_HANDLER(PpapiMsg_InitializeModule, OnMsgInitializeModule)
      IPC_MESSAGE_HANDLER(PpapiMsg_Shutdown, OnMsgShutdown)

      // Forward all other control messages to the superclass.
      IPC_MESSAGE_UNHANDLED(handled = Dispatcher::OnMessageReceived(msg))
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  // All non-control messages get handled by the superclass.
  return Dispatcher::OnMessageReceived(msg);
}

void PluginDispatcher::DidCreateInstance(PP_Instance instance) {
  instance_map_[instance] = InstanceData();
}

void PluginDispatcher::DidDestroyInstance(PP_Instance instance) {
  InstanceDataMap::iterator it = instance_map_.find(instance);
  if (it != instance_map_.end())
    instance_map_.erase(it);
}

InstanceData* PluginDispatcher::GetInstanceData(PP_Instance instance) {
  InstanceDataMap::iterator it = instance_map_.find(instance);
  return (it == instance_map_.end()) ? NULL : &it->second;
}

void PluginDispatcher::OnMsgInitializeModule(PP_Module pp_module,
                                             bool* result) {
  set_pp_module(pp_module);
  *result = init_module_(pp_module, &GetInterfaceFromDispatcher) == PP_OK;
}

void PluginDispatcher::OnMsgShutdown() {
  if (shutdown_module_)
    shutdown_module_();
  MessageLoop::current()->Quit();
}

}  // namespace proxy
}  // namespace pp

