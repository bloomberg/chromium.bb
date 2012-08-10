// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

PluginResource::PluginResource(Connection connection, PP_Instance instance)
    : Resource(OBJECT_IS_PROXY, instance),
      connection_(connection),
      next_sequence_number_(0),
      sent_create_to_browser_(false),
      sent_create_to_renderer_(false) {
}

PluginResource::~PluginResource() {
  if (sent_create_to_browser_) {
    connection_.browser_sender->Send(
        new PpapiHostMsg_ResourceDestroyed(pp_resource()));
  }
  if (sent_create_to_renderer_) {
    connection_.renderer_sender->Send(
        new PpapiHostMsg_ResourceDestroyed(pp_resource()));
  }
}

void PluginResource::SendCreateToBrowser(const IPC::Message& msg) {
  DCHECK(!sent_create_to_browser_);
  sent_create_to_browser_ = true;
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  connection_.browser_sender->Send(
      new PpapiHostMsg_ResourceCreated(params, pp_instance(), msg));
}

void PluginResource::SendCreateToRenderer(const IPC::Message& msg) {
  DCHECK(!sent_create_to_renderer_);
  sent_create_to_renderer_ = true;
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  connection_.renderer_sender->Send(
      new PpapiHostMsg_ResourceCreated(params, pp_instance(), msg));
}

void PluginResource::PostToBrowser(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  connection_.browser_sender->Send(new PpapiHostMsg_ResourceCall(params, msg));
}

void PluginResource::PostToRenderer(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  connection_.renderer_sender->Send(new PpapiHostMsg_ResourceCall(params, msg));
}

int32_t PluginResource::CallBrowser(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  params.set_has_callback();
  connection_.browser_sender->Send(new PpapiHostMsg_ResourceCall(params, msg));
  return params.sequence();
}

int32_t PluginResource::CallRenderer(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  params.set_has_callback();
  connection_.renderer_sender->Send(new PpapiHostMsg_ResourceCall(params, msg));
  return params.sequence();
}

}  // namespace proxy
}  // namespace ppapi
