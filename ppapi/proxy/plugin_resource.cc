// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource.h"

#include "ppapi/c/pp_errors.h"
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

void PluginResource::OnReplyReceived(
    const proxy::ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  // Grab the callback for the reply sequence number and run it with |msg|.
  CallbackMap::iterator it = callbacks_.find(params.sequence());
  if (it == callbacks_.end()) {
    DCHECK(false) << "Callback does not exist for an expected sequence number.";
  } else {
    scoped_refptr<PluginResourceCallbackBase> callback = it->second;
    callbacks_.erase(it);
    callback->Run(params, msg);
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
  SendResourceCall(connection_.browser_sender, params, msg);
}

void PluginResource::PostToRenderer(const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  SendResourceCall(connection_.renderer_sender, params, msg);
}

bool PluginResource::SendResourceCall(
    IPC::Sender* sender,
    const ResourceMessageCallParams& call_params,
    const IPC::Message& nested_msg) {
  return sender->Send(new PpapiHostMsg_ResourceCall(call_params, nested_msg));
}

int32_t PluginResource::CallBrowserSync(const IPC::Message& msg,
                                        IPC::Message* reply) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  params.set_has_callback();
  ResourceMessageReplyParams reply_params;
  bool success =
      connection_.browser_sender->Send(new PpapiHostMsg_ResourceSyncCall(
          params, msg, &reply_params, reply));
  if (success)
    return reply_params.result();
  return PP_ERROR_FAILED;
}

int32_t PluginResource::CallRendererSync(const IPC::Message& msg,
                                         IPC::Message* reply) {
  ResourceMessageCallParams params(pp_resource(),
                                   next_sequence_number_++);
  params.set_has_callback();
  ResourceMessageReplyParams reply_params;
  bool success =
      connection_.renderer_sender->Send(new PpapiHostMsg_ResourceSyncCall(
          params, msg, &reply_params, reply));
  if (success)
    return reply_params.result();
  return PP_ERROR_FAILED;
}

}  // namespace proxy
}  // namespace ppapi
