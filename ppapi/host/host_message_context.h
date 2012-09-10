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

// This context structure provides information about incoming resource message
// call requests when passed to resources.
struct PPAPI_HOST_EXPORT HostMessageContext {
  explicit HostMessageContext(
      const ppapi::proxy::ResourceMessageCallParams& cp);
  ~HostMessageContext();

  // Returns a "reply params" struct with the same resource and sequence number
  // as this request.
  ppapi::proxy::ResourceMessageReplyParams MakeReplyParams();

  // The original call parameters passed to the resource message call.
  const ppapi::proxy::ResourceMessageCallParams& params;

  // The reply message. If the params has the callback flag set, this message
  // will be sent in reply. It is initialized to the empty message. If the
  // handler wants to send something else, it should just assign the message
  // it wants to this value.
  IPC::Message reply_msg;
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_HOST_MESSAGE_CONTEXT_H_
