// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource.h"

#include "ppapi/proxy/ppapi_messages.h"

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

void PluginResource::SendCreate(Destination dest, const IPC::Message& msg) {
  if (dest == RENDERER) {
    DCHECK(!sent_create_to_renderer_);
    sent_create_to_renderer_ = true;
  } else {
    DCHECK(!sent_create_to_browser_);
    sent_create_to_browser_ = true;
  }
  ResourceMessageCallParams params(pp_resource(), next_sequence_number_++);
  GetSender(dest)->Send(
      new PpapiHostMsg_ResourceCreated(params, pp_instance(), msg));
}

void PluginResource::Post(Destination dest, const IPC::Message& msg) {
  ResourceMessageCallParams params(pp_resource(), next_sequence_number_++);
  SendResourceCall(dest, params, msg);
}

bool PluginResource::SendResourceCall(
    Destination dest,
    const ResourceMessageCallParams& call_params,
    const IPC::Message& nested_msg) {
  return GetSender(dest)->Send(
      new PpapiHostMsg_ResourceCall(call_params, nested_msg));
}

int32_t PluginResource::GenericSyncCall(Destination dest,
                                        const IPC::Message& msg,
                                        IPC::Message* reply) {
  ResourceMessageCallParams params(pp_resource(), next_sequence_number_++);
  params.set_has_callback();
  ResourceMessageReplyParams reply_params;
  bool success = GetSender(dest)->Send(new PpapiHostMsg_ResourceSyncCall(
      params, msg, &reply_params, reply));
  if (success)
    return reply_params.result();
  return PP_ERROR_FAILED;
}

}  // namespace proxy
}  // namespace ppapi
