// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_HOST_MESSAGE_CONTEXT_H_
#define PPAPI_HOST_HOST_MESSAGE_CONTEXT_H_

#include "ipc/ipc_message.h"
#include "ppapi/host/ppapi_host_export.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace host {

// This context structure provides information about outgoing resource message
// replies.
struct PPAPI_HOST_EXPORT ReplyMessageContext {
  ReplyMessageContext();
  ReplyMessageContext(
      const ppapi::proxy::ResourceMessageReplyParams& cp,
      IPC::Message* sync_reply_msg);
  ~ReplyMessageContext();

  // The "reply params" struct with the same resource and sequence number
  // as the original resource message call.
  ppapi::proxy::ResourceMessageReplyParams params;

  // If this context is generated from a sync message, this will be set to the
  // incoming sync message. Otherwise, it will be NULL. The plugin controls
  // whether or not the resource call is synchronous or asynchronous so a
  // ResoureHost cannot make any assumptions about whether or not this is NULL.
  IPC::Message* sync_reply_msg;
};

// This context structure provides information about incoming resource message
// call requests when passed to resources.
struct PPAPI_HOST_EXPORT HostMessageContext {
  explicit HostMessageContext(
      const ppapi::proxy::ResourceMessageCallParams& cp);
  HostMessageContext(
      const ppapi::proxy::ResourceMessageCallParams& cp,
      IPC::Message* sync_reply_msg);
  ~HostMessageContext();

  // Returns a reply message context struct which includes the reply params.
  ReplyMessageContext MakeReplyMessageContext() const;

  // The original call parameters passed to the resource message call. This
  // cannot be a reference because this object may be passed to another thread.
  ppapi::proxy::ResourceMessageCallParams params;

  // The reply message. If the params has the callback flag set, this message
  // will be sent in reply. It is initialized to the empty message. If the
  // handler wants to send something else, it should just assign the message
  // it wants to this value.
  IPC::Message reply_msg;

  // If this context is generated from a sync message, this will be set to the
  // incoming sync message. Otherwise, it will be NULL.
  IPC::Message* sync_reply_msg;
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_HOST_MESSAGE_CONTEXT_H_
