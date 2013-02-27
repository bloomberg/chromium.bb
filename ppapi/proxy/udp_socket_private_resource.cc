// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_private_resource.h"

#include <algorithm>
#include <cstring>

#include "base/basictypes.h"
#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

const int32_t UDPSocketPrivateResource::kMaxReadSize = 1024 * 1024;
const int32_t UDPSocketPrivateResource::kMaxWriteSize = 1024 * 1024;

UDPSocketPrivateResource::UDPSocketPrivateResource(Connection connection,
                                                   PP_Instance instance)
    : PluginResource(connection, instance),
      bound_(false),
      closed_(false) {
  recvfrom_addr_.size = 0;
  memset(recvfrom_addr_.data, 0,
         arraysize(recvfrom_addr_.data) * sizeof(*recvfrom_addr_.data));
  bound_addr_.size = 0;
  memset(bound_addr_.data, 0,
         arraysize(bound_addr_.data) * sizeof(*bound_addr_.data));

  SendCreate(BROWSER, PpapiHostMsg_UDPSocketPrivate_Create());
}

UDPSocketPrivateResource::~UDPSocketPrivateResource() {
}

thunk::PPB_UDPSocket_Private_API*
UDPSocketPrivateResource::AsPPB_UDPSocket_Private_API() {
  return this;
}

int32_t UDPSocketPrivateResource::SetSocketFeature(
    PP_UDPSocketFeature_Private name,
    PP_Var value) {
  if (bound_ || closed_)
    return PP_ERROR_FAILED;

  switch (name) {
    case PP_UDPSOCKETFEATURE_ADDRESS_REUSE:
    case PP_UDPSOCKETFEATURE_BROADCAST:
      if (value.type != PP_VARTYPE_BOOL)
        return PP_ERROR_BADARGUMENT;
      SendBoolSocketFeature(static_cast<int32_t>(name),
                            PP_ToBool(value.value.as_bool));
      break;
    default:
      return PP_ERROR_BADARGUMENT;
  }
  return PP_OK;
}

int32_t UDPSocketPrivateResource::Bind(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (bound_ || closed_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(bind_callback_))
    return PP_ERROR_INPROGRESS;

  bind_callback_ = callback;

  // Send the request, the browser will call us back via BindReply.
  SendBind(*addr);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocketPrivateResource::GetBoundAddress(PP_NetAddress_Private* addr) {
  if (!addr || !bound_ || closed_)
    return PP_FALSE;

  *addr = bound_addr_;
  return PP_TRUE;
}

int32_t UDPSocketPrivateResource::RecvFrom(
    char* buffer,
    int32_t num_bytes,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || num_bytes <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!bound_)
    return PP_ERROR_FAILED;

  recvfrom_requests_.push(RecvFromRequest(callback,
                                          buffer,
                                          std::min(num_bytes, kMaxReadSize)));

  // Send the request, the browser will call us back via RecvFromReply.
  SendRecvFrom(num_bytes);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocketPrivateResource::GetRecvFromAddress(
    PP_NetAddress_Private* addr) {
  if (!addr)
    return PP_FALSE;
  *addr = recvfrom_addr_;
  return PP_TRUE;
}

void UDPSocketPrivateResource::PostAbortIfNecessary(
    scoped_refptr<TrackedCallback>* callback) {
  if (TrackedCallback::IsPending(*callback))
    (*callback)->PostAbort();
}

int32_t UDPSocketPrivateResource::SendTo(
    const char* buffer,
    int32_t num_bytes,
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || num_bytes <= 0 || !addr)
    return PP_ERROR_BADARGUMENT;
  if (!bound_)
    return PP_ERROR_FAILED;

  if (num_bytes > kMaxWriteSize)
    num_bytes = kMaxWriteSize;

  sendto_callbacks_.push(callback);

  // Send the request, the browser will call us back via SendToReply.
  SendSendTo(std::string(buffer, num_bytes), *addr);
  return PP_OK_COMPLETIONPENDING;
}

void UDPSocketPrivateResource::Close() {
  if(closed_)
    return;

  bound_ = false;
  closed_ = true;

  SendClose();

  PostAbortIfNecessary(&bind_callback_);
  while (!recvfrom_requests_.empty()) {
    RecvFromRequest& request = recvfrom_requests_.front();
    PostAbortIfNecessary(&request.callback);
    recvfrom_requests_.pop();
  }
  while (!sendto_callbacks_.empty()) {
    PostAbortIfNecessary(&sendto_callbacks_.front());
    sendto_callbacks_.pop();
  }
}

void UDPSocketPrivateResource::SendBoolSocketFeature(int32_t name, bool value) {
  PpapiHostMsg_UDPSocketPrivate_SetBoolSocketFeature msg(name, value);
  Post(BROWSER, msg);
}

void UDPSocketPrivateResource::SendBind(const PP_NetAddress_Private& addr) {
  PpapiHostMsg_UDPSocketPrivate_Bind msg(addr);
  Call<PpapiPluginMsg_UDPSocketPrivate_BindReply>(
      BROWSER,
      msg,
      base::Bind(&UDPSocketPrivateResource::OnPluginMsgBindReply,
                 base::Unretained(this)));
}

void UDPSocketPrivateResource::SendRecvFrom(int32_t num_bytes) {
  PpapiHostMsg_UDPSocketPrivate_RecvFrom msg(num_bytes);
  Call<PpapiPluginMsg_UDPSocketPrivate_RecvFromReply>(
      BROWSER,
      msg,
      base::Bind(&UDPSocketPrivateResource::OnPluginMsgRecvFromReply,
                 base::Unretained(this)));
}

void UDPSocketPrivateResource::SendSendTo(const std::string& buffer,
                                          const PP_NetAddress_Private& addr) {
  PpapiHostMsg_UDPSocketPrivate_SendTo msg(buffer, addr);
  Call<PpapiPluginMsg_UDPSocketPrivate_SendToReply>(
      BROWSER,
      msg,
      base::Bind(&UDPSocketPrivateResource::OnPluginMsgSendToReply,
                 base::Unretained(this)));
}

void UDPSocketPrivateResource::SendClose() {
  PpapiHostMsg_UDPSocketPrivate_Close msg;
  Post(BROWSER, msg);
}

void UDPSocketPrivateResource::OnPluginMsgBindReply(
    const ResourceMessageReplyParams& params,
    const PP_NetAddress_Private& bound_addr) {
  if (!TrackedCallback::IsPending(bind_callback_)) {
    NOTREACHED();
    return;
  }
  if (params.result() == PP_OK)
    bound_ = true;
  bound_addr_ = bound_addr;
  bind_callback_->Run(params.result());
}

void UDPSocketPrivateResource::OnPluginMsgRecvFromReply(
    const ResourceMessageReplyParams& params,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  if (recvfrom_requests_.empty())
    return;
  RecvFromRequest request = recvfrom_requests_.front();
  recvfrom_requests_.pop();
  if (!TrackedCallback::IsPending(request.callback) || !request.buffer) {
    NOTREACHED();
    return;
  }
  bool succeeded = (params.result() == PP_OK);
  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), request.num_bytes);
    if (!data.empty())
      memcpy(request.buffer, data.c_str(), data.size());
  }

  recvfrom_addr_ = addr;

  if (succeeded)
    request.callback->Run(static_cast<int32_t>(data.size()));
  else
    request.callback->Run(params.result());
}

void UDPSocketPrivateResource::OnPluginMsgSendToReply(
    const ResourceMessageReplyParams& params,
    int32_t bytes_written) {
  if (sendto_callbacks_.empty())
    return;
  scoped_refptr<TrackedCallback> callback  = sendto_callbacks_.front();
  sendto_callbacks_.pop();
  if (!TrackedCallback::IsPending(callback)) {
    NOTREACHED();
    return;
  }
  if (params.result() == PP_OK)
    callback->Run(bytes_written);
  else
    callback->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
