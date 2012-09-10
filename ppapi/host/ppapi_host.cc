// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/ppapi_host.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_factory.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/instance_message_filter.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace host {

namespace {

// Put a cap on the maximum number of resources so we don't explode if the
// renderer starts spamming us.
const size_t kMaxResourcesPerPlugin = 1 << 14;

}  // namespace

PpapiHost::PpapiHost(IPC::Sender* sender,
                     HostFactory* host_factory,
                     const PpapiPermissions& perms)
    : sender_(sender),
      host_factory_(host_factory),
      permissions_(perms) {
}

PpapiHost::~PpapiHost() {
  // Delete these explicitly before destruction since then the host is still
  // technically alive in case one of the filters accesses us from the
  // destructor.
  instance_message_filters_.clear();
}

bool PpapiHost::Send(IPC::Message* msg) {
  return sender_->Send(msg);
}

bool PpapiHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PpapiHost, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ResourceCall,
                        OnHostMsgResourceCall)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ResourceCreated,
                        OnHostMsgResourceCreated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_ResourceDestroyed,
                        OnHostMsgResourceDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    for (size_t i = 0; i < instance_message_filters_.size(); i++) {
      if (instance_message_filters_[i]->OnInstanceMessageReceived(msg)) {
        handled = true;
        break;
      }
    }
  }

  return handled;
}

void PpapiHost::SendReply(const proxy::ResourceMessageReplyParams& params,
                          const IPC::Message& msg) {
  Send(new PpapiPluginMsg_ResourceReply(params, msg));
}


void PpapiHost::AddInstanceMessageFilter(
    scoped_ptr<InstanceMessageFilter> filter) {
  instance_message_filters_.push_back(filter.release());
}

void PpapiHost::OnHostMsgResourceCall(
    const proxy::ResourceMessageCallParams& params,
    const IPC::Message& nested_msg) {
  HostMessageContext context(params);
  proxy::ResourceMessageReplyParams reply_params(params.pp_resource(),
                                                 params.sequence());

  ResourceHost* resource_host = GetResourceHost(params.pp_resource());
  if (resource_host) {
    reply_params.set_result(resource_host->OnResourceMessageReceived(
        nested_msg, &context));

    // Sanity check the resource handler. Note if the result was
    // "completion pending" the resource host may have already sent the reply.
    if (reply_params.result() == PP_OK_COMPLETIONPENDING) {
      // Message handler should have only returned a pending result if a
      // response will be sent to the plugin.
      DCHECK(params.has_callback());

      // Message handler should not have written a message to be returned if
      // completion is pending.
      DCHECK(context.reply_msg.type() == 0);
    } else if (!params.has_callback()) {
      // When no response is required, the message handler should not have
      // written a message to be returned.
      DCHECK(context.reply_msg.type() == 0);
    }
  } else {
    reply_params.set_result(PP_ERROR_BADRESOURCE);
  }

  if (params.has_callback() && reply_params.result() != PP_OK_COMPLETIONPENDING)
    SendReply(reply_params, context.reply_msg);
}

void PpapiHost::OnHostMsgResourceCreated(
    const proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const IPC::Message& nested_msg) {
  if (resources_.size() >= kMaxResourcesPerPlugin)
    return;

  scoped_ptr<ResourceHost> resource_host(
      host_factory_->CreateResourceHost(this, params, instance, nested_msg));
  if (!resource_host.get()) {
    NOTREACHED();
    return;
  }

  resources_[params.pp_resource()] =
      linked_ptr<ResourceHost>(resource_host.release());
}

void PpapiHost::OnHostMsgResourceDestroyed(PP_Resource resource) {
  ResourceMap::iterator found = resources_.find(resource);
  if (found == resources_.end()) {
    NOTREACHED();
    return;
  }
  resources_.erase(found);
}

ResourceHost* PpapiHost::GetResourceHost(PP_Resource resource) {
  ResourceMap::iterator found = resources_.find(resource);
  return found == resources_.end() ? NULL : found->second.get();
}

}  // namespace host
}  // namespace ppapi
