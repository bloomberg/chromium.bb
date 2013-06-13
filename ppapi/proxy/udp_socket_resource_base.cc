// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/udp_socket_resource_base.h"

#include <algorithm>
#include <cstring>

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace proxy {

const int32_t UDPSocketResourceBase::kMaxReadSize = 1024 * 1024;
const int32_t UDPSocketResourceBase::kMaxWriteSize = 1024 * 1024;

UDPSocketResourceBase::UDPSocketResourceBase(Connection connection,
                                             PP_Instance instance)
    : PluginResource(connection, instance),
      bound_(false),
      closed_(false),
      read_buffer_(NULL),
      bytes_to_read_(-1) {
  recvfrom_addr_.size = 0;
  memset(recvfrom_addr_.data, 0,
         arraysize(recvfrom_addr_.data) * sizeof(*recvfrom_addr_.data));
  bound_addr_.size = 0;
  memset(bound_addr_.data, 0,
         arraysize(bound_addr_.data) * sizeof(*bound_addr_.data));

  SendCreate(BROWSER, PpapiHostMsg_UDPSocketPrivate_Create());
}

UDPSocketResourceBase::~UDPSocketResourceBase() {
}

int32_t UDPSocketResourceBase::SetSocketFeatureImpl(
    PP_UDPSocketFeature_Private name,
    const PP_Var& value) {
  if (bound_ || closed_)
    return PP_ERROR_FAILED;

  switch (name) {
    case PP_UDPSOCKETFEATURE_ADDRESS_REUSE:
    case PP_UDPSOCKETFEATURE_BROADCAST: {
      if (value.type != PP_VARTYPE_BOOL)
        return PP_ERROR_BADARGUMENT;
      Post(BROWSER,
           PpapiHostMsg_UDPSocketPrivate_SetBoolSocketFeature(
               static_cast<int32_t>(name), PP_ToBool(value.value.as_bool)));
      break;
    }
    default: {
      return PP_ERROR_BADARGUMENT;
    }
  }
  return PP_OK;
}

int32_t UDPSocketResourceBase::BindImpl(
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
  Call<PpapiPluginMsg_UDPSocketPrivate_BindReply>(
      BROWSER,
      PpapiHostMsg_UDPSocketPrivate_Bind(*addr),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgBindReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocketResourceBase::GetBoundAddressImpl(
    PP_NetAddress_Private* addr) {
  if (!addr || !bound_ || closed_)
    return PP_FALSE;

  *addr = bound_addr_;
  return PP_TRUE;
}

int32_t UDPSocketResourceBase::RecvFromImpl(
    char* buffer,
    int32_t num_bytes,
    PP_Resource* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || num_bytes <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!bound_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(recvfrom_callback_))
    return PP_ERROR_INPROGRESS;

  read_buffer_ = buffer;
  bytes_to_read_ = std::min(num_bytes, kMaxReadSize);
  recvfrom_callback_ = callback;

  // Send the request, the browser will call us back via RecvFromReply.
  Call<PpapiPluginMsg_UDPSocketPrivate_RecvFromReply>(
      BROWSER,
      PpapiHostMsg_UDPSocketPrivate_RecvFrom(bytes_to_read_),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgRecvFromReply,
                 base::Unretained(this), addr));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool UDPSocketResourceBase::GetRecvFromAddressImpl(
    PP_NetAddress_Private* addr) {
  if (!addr)
    return PP_FALSE;
  *addr = recvfrom_addr_;
  return PP_TRUE;
}

int32_t UDPSocketResourceBase::SendToImpl(
    const char* buffer,
    int32_t num_bytes,
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || num_bytes <= 0 || !addr)
    return PP_ERROR_BADARGUMENT;
  if (!bound_)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(sendto_callback_))
    return PP_ERROR_INPROGRESS;

  if (num_bytes > kMaxWriteSize)
    num_bytes = kMaxWriteSize;

  sendto_callback_ = callback;

  // Send the request, the browser will call us back via SendToReply.
  Call<PpapiPluginMsg_UDPSocketPrivate_SendToReply>(
      BROWSER,
      PpapiHostMsg_UDPSocketPrivate_SendTo(
          std::string(buffer, num_bytes), *addr),
      base::Bind(&UDPSocketResourceBase::OnPluginMsgSendToReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

void UDPSocketResourceBase::CloseImpl() {
  if(closed_)
    return;

  bound_ = false;
  closed_ = true;

  Post(BROWSER, PpapiHostMsg_UDPSocketPrivate_Close());

  PostAbortIfNecessary(&bind_callback_);
  PostAbortIfNecessary(&recvfrom_callback_);
  PostAbortIfNecessary(&sendto_callback_);
}

void UDPSocketResourceBase::PostAbortIfNecessary(
    scoped_refptr<TrackedCallback>* callback) {
  if (TrackedCallback::IsPending(*callback))
    (*callback)->PostAbort();
}

void UDPSocketResourceBase::OnPluginMsgBindReply(
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

void UDPSocketResourceBase::OnPluginMsgRecvFromReply(
    PP_Resource* output_addr,
    const ResourceMessageReplyParams& params,
    const std::string& data,
    const PP_NetAddress_Private& addr) {
  if (!TrackedCallback::IsPending(recvfrom_callback_)) {
    NOTREACHED();
    return;
  }
  bool succeeded = (params.result() == PP_OK);
  if (succeeded && output_addr) {
    thunk::EnterResourceCreationNoLock enter(pp_instance());
    if (enter.succeeded()) {
      *output_addr = enter.functions()->CreateNetAddressFromNetAddressPrivate(
          pp_instance(), addr);
    } else {
      succeeded = false;
    }
  }

  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memcpy(read_buffer_, data.c_str(), data.size());
  }

  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  recvfrom_addr_ = addr;

  if (succeeded)
    recvfrom_callback_->Run(static_cast<int32_t>(data.size()));
  else
    recvfrom_callback_->Run(params.result());
}

void UDPSocketResourceBase::OnPluginMsgSendToReply(
    const ResourceMessageReplyParams& params,
    int32_t bytes_written) {
  if (!TrackedCallback::IsPending(sendto_callback_)) {
    NOTREACHED();
    return;
  }
  if (params.result() == PP_OK)
    sendto_callback_->Run(bytes_written);
  else
    sendto_callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
