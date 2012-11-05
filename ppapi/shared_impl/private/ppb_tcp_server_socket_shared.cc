// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/ppb_tcp_server_socket_shared.h"

#include <cstddef>

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {

PPB_TCPServerSocket_Shared::PPB_TCPServerSocket_Shared(PP_Instance instance)
    : Resource(OBJECT_IS_IMPL, instance),
      socket_id_(0),
      state_(BEFORE_LISTENING),
      tcp_socket_buffer_(NULL) {
}

PPB_TCPServerSocket_Shared::PPB_TCPServerSocket_Shared(
    const HostResource& resource)
    : Resource(OBJECT_IS_PROXY, resource),
      socket_id_(0),
      state_(BEFORE_LISTENING),
      tcp_socket_buffer_(NULL) {
}

PPB_TCPServerSocket_Shared::~PPB_TCPServerSocket_Shared() {
}

thunk::PPB_TCPServerSocket_Private_API*
PPB_TCPServerSocket_Shared::AsPPB_TCPServerSocket_Private_API() {
  return this;
}

int32_t PPB_TCPServerSocket_Shared::Listen(
    const PP_NetAddress_Private* addr,
    int32_t backlog,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (state_ != BEFORE_LISTENING)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(listen_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  listen_callback_ = callback;
  // Send the request, the browser will call us back via ListenACK
  SendListen(*addr, backlog);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_TCPServerSocket_Shared::Accept(
    PP_Resource* tcp_socket,
    scoped_refptr<TrackedCallback> callback) {
  if (!tcp_socket)
    return PP_ERROR_BADARGUMENT;

  if (state_ != LISTENING)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(accept_callback_))
    return PP_ERROR_INPROGRESS;

  tcp_socket_buffer_ = tcp_socket;
  accept_callback_ = callback;

  SendAccept();
  return PP_OK_COMPLETIONPENDING;
}

void PPB_TCPServerSocket_Shared::StopListening() {
  if (state_ == CLOSED)
    return;

  state_ = CLOSED;

  SendStopListening();
  socket_id_ = 0;

  if (TrackedCallback::IsPending(listen_callback_))
    listen_callback_->PostAbort();
  if (TrackedCallback::IsPending(accept_callback_))
    accept_callback_->PostAbort();
  tcp_socket_buffer_ = NULL;
}

void PPB_TCPServerSocket_Shared::OnListenCompleted(uint32 socket_id,
                                                   int32_t status) {
  if (state_ != BEFORE_LISTENING ||
      !TrackedCallback::IsPending(listen_callback_)) {
    NOTREACHED();
    return;
  }

  if (status == PP_OK) {
    socket_id_ = socket_id;
    state_ = LISTENING;
  }

  listen_callback_->Run(status);
}

}  // namespace ppapi
