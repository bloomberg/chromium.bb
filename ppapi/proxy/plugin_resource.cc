// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

PluginResource::PluginResource(IPC::Sender* sender, PP_Instance instance)
    : Resource(OBJECT_IS_PROXY, instance),
      sender_(sender),
      next_sequence_number_(0),
      sent_create_to_renderer_(false) {
}

PluginResource::~PluginResource() {
  if (sent_create_to_renderer_)
    Send(new PpapiHostMsg_ResourceDestroyed(pp_resource()));
}

bool PluginResource::Send(IPC::Message* message) {
  return sender_->Send(message);
}

void PluginResource::SendCreateToRenderer(const IPC::Message& msg) {
  DCHECK(!sent_create_to_renderer_);
  sent_create_to_renderer_ = true;
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  Send(new PpapiHostMsg_ResourceCreated(params, pp_instance(), msg));
}

void PluginResource::PostToRenderer(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  Send(new PpapiHostMsg_ResourceCall(params, msg));
}

int32_t PluginResource::CallRenderer(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  params.set_has_callback();
  Send(new PpapiHostMsg_ResourceCall(params, msg));
  return params.sequence();
}

}  // namespace proxy
}  // namespace ppapi
