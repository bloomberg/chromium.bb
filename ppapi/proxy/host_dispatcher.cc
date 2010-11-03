// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/host_dispatcher.h"

#include <map>

#include "base/logging.h"
#include "ppapi/proxy/host_var_serialization_rules.h"

namespace pp {
namespace proxy {

namespace {

typedef std::map<PP_Instance, HostDispatcher*> InstanceToDispatcherMap;
InstanceToDispatcherMap* g_instance_to_dispatcher = NULL;

}  // namespace

HostDispatcher::HostDispatcher(const PPB_Var_Deprecated* var_interface,
                               PP_Module module,
                               GetInterfaceFunc local_get_interface)
    : Dispatcher(local_get_interface) {
  SetSerializationRules(new HostVarSerializationRules(var_interface, module));
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

}  // namespace proxy
}  // namespace pp

