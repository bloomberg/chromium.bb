// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_dispatcher.h"

#include <map>

#include "base/logging.h"
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
    : Dispatcher(remote_process_handle, local_get_interface) {
  set_pp_module(module);
  const PPB_Var_Deprecated* var_interface =
      static_cast<const PPB_Var_Deprecated*>(
          local_get_interface(PPB_VAR_DEPRECATED_INTERFACE));
  SetSerializationRules(new HostVarSerializationRules(var_interface, module));
}

HostDispatcher::~HostDispatcher() {
  // Notify the plugin that it should exit.
  Send(new PpapiMsg_Shutdown());
}

bool HostDispatcher::InitializeModule() {
  bool init_result = false;
  Send(new PpapiMsg_InitializeModule(pp_module(), &init_result));
  return init_result;
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

}  // namespace proxy
}  // namespace pp

