// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_message_filter.h"

#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

PluginMessageFilter::PluginMessageFilter(
    std::set<PP_Instance>* seen_instance_ids)
    : seen_instance_ids_(seen_instance_ids),
      channel_(NULL) {
}

PluginMessageFilter::~PluginMessageFilter() {
}

void PluginMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void PluginMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

bool PluginMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginMessageFilter, message)
    IPC_MESSAGE_HANDLER(PpapiMsg_ReserveInstanceId, OnMsgReserveInstanceId)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool PluginMessageFilter::Send(IPC::Message* msg) {
  if (channel_)
    return channel_->Send(msg);
  delete msg;
  return false;
}

void PluginMessageFilter::OnMsgReserveInstanceId(PP_Instance instance,
                                                 bool* usable) {
  // See the message definition for how this works.
  if (seen_instance_ids_->find(instance) != seen_instance_ids_->end()) {
    // Instance ID already seen, reject it.
    *usable = false;
    return;
  }

  // This instance ID is new so we can return that it's usable and mark it as
  // used for future reference.
  seen_instance_ids_->insert(instance);
  *usable = true;
}

}  // namespace proxy
}  // namespace pp
