// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_dispatcher.h"

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_channel.h"
#include "base/debug/trace_event.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_serialization_rules.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_char_set_proxy.h"
#include "ppapi/proxy/ppb_cursor_control_proxy.h"
#include "ppapi/proxy/ppb_font_proxy.h"
#include "ppapi/proxy/ppb_instance_proxy.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/resource_creation_proxy.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracker_base.h"

#if defined(OS_POSIX)
#include "base/eintr_wrapper.h"
#include "ipc/ipc_channel_posix.h"
#endif

namespace ppapi {
namespace proxy {

namespace {

typedef std::map<PP_Instance, PluginDispatcher*> InstanceToDispatcherMap;
InstanceToDispatcherMap* g_instance_to_dispatcher = NULL;

}  // namespace

PluginDispatcher::PluginDispatcher(base::ProcessHandle remote_process_handle,
                                   GetInterfaceFunc get_interface)
    : Dispatcher(remote_process_handle, get_interface),
      plugin_delegate_(NULL),
      received_preferences_(false),
      plugin_dispatcher_id_(0) {
  SetSerializationRules(new PluginVarSerializationRules);
  TrackerBase::Init(&PluginResourceTracker::GetTrackerBaseInstance);
}

PluginDispatcher::~PluginDispatcher() {
  if (plugin_delegate_)
    plugin_delegate_->Unregister(plugin_dispatcher_id_);
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
PluginDispatcher* PluginDispatcher::GetForResource(const Resource* resource) {
  return GetForInstance(resource->pp_instance());
}

// static
const void* PluginDispatcher::GetBrowserInterface(const char* interface) {
  return InterfaceList::GetInstance()->GetInterfaceForPPB(interface);
}

const void* PluginDispatcher::GetPluginInterface(
    const std::string& interface_name) {
  InterfaceMap::iterator found = plugin_interfaces_.find(interface_name);
  if (found == plugin_interfaces_.end()) {
    const void* ret = local_get_interface()(interface_name.c_str());
    plugin_interfaces_.insert(std::make_pair(interface_name, ret));
    return ret;
  }
  return found->second;
}

bool PluginDispatcher::InitPluginWithChannel(
    PluginDelegate* delegate,
    const IPC::ChannelHandle& channel_handle,
    bool is_client) {
  if (!Dispatcher::InitWithChannel(delegate, channel_handle, is_client))
    return false;
  plugin_delegate_ = delegate;
  plugin_dispatcher_id_ = plugin_delegate_->Register(this);

  // The message filter will intercept and process certain messages directly
  // on the I/O thread.
  channel()->AddFilter(
      new PluginMessageFilter(delegate->GetGloballySeenInstanceIDSet()));
  return true;
}

bool PluginDispatcher::IsPlugin() const {
  return true;
}

bool PluginDispatcher::Send(IPC::Message* msg) {
  TRACE_EVENT2("ppapi proxy", "PluginDispatcher::Send",
               "Class", IPC_MESSAGE_ID_CLASS(msg->type()),
               "Line", IPC_MESSAGE_ID_LINE(msg->type()));
  // We always want plugin->renderer messages to arrive in-order. If some sync
  // and some async messages are send in response to a synchronous
  // renderer->plugin call, the sync reply will be processed before the async
  // reply, and everything will be confused.
  //
  // Allowing all async messages to unblock the renderer means more reentrancy
  // there but gives correct ordering.
  msg->set_unblock(true);
  return Dispatcher::Send(msg);
}

bool PluginDispatcher::OnMessageReceived(const IPC::Message& msg) {
  TRACE_EVENT2("ppapi proxy", "PluginDispatcher::OnMessageReceived",
               "Class", IPC_MESSAGE_ID_CLASS(msg.type()),
               "Line", IPC_MESSAGE_ID_LINE(msg.type()));
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    // Handle some plugin-specific control messages.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PluginDispatcher, msg)
      IPC_MESSAGE_HANDLER(PpapiMsg_SupportsInterface, OnMsgSupportsInterface)
      IPC_MESSAGE_HANDLER(PpapiMsg_SetPreferences, OnMsgSetPreferences)
      IPC_MESSAGE_UNHANDLED(handled = false);
    IPC_END_MESSAGE_MAP()
    if (handled)
      return true;
  }
  return Dispatcher::OnMessageReceived(msg);
}

void PluginDispatcher::OnChannelError() {
  Dispatcher::OnChannelError();

  // The renderer has crashed or exited. This channel and all instances
  // associated with it are no longer valid.
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

void PluginDispatcher::PostToWebKitThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return plugin_delegate_->PostToWebKitThread(from_here, task);
}

bool PluginDispatcher::SendToBrowser(IPC::Message* msg) {
  return plugin_delegate_->SendToBrowser(msg);
}

WebKitForwarding* PluginDispatcher::GetWebKitForwarding() {
  return plugin_delegate_->GetWebKitForwarding();
}

FunctionGroupBase* PluginDispatcher::GetFunctionAPI(InterfaceID id) {
  return GetInterfaceProxy(id);
}

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
      PpapiMsg_PPPInstance_DidDestroy msg(INTERFACE_ID_PPP_INSTANCE, i->first);
      OnMessageReceived(msg);
    }
  }
}

void PluginDispatcher::OnMsgSupportsInterface(
    const std::string& interface_name,
    bool* result) {
  *result = !!GetPluginInterface(interface_name);
}

void PluginDispatcher::OnMsgSetPreferences(const Preferences& prefs) {
  // The renderer may send us preferences more than once (currently this
  // happens every time a new plugin instance is created). Since we don't have
  // a way to signal to the plugin that the preferences have changed, changing
  // the default fonts and such in the middle of a running plugin could be
  // confusing to it. As a result, we never allow the preferences to be changed
  // once they're set. The user will have to restart to get new font prefs
  // propogated to plugins.
  if (!received_preferences_) {
    received_preferences_ = true;
    preferences_ = prefs;
  }
}

}  // namespace proxy
}  // namespace ppapi
