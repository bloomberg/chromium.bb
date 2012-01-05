// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"

#include <string.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {

const int32_t TCPSocketPrivateImpl::kMaxReadSize = 1024 * 1024;
const int32_t TCPSocketPrivateImpl::kMaxWriteSize = 1024 * 1024;

TCPSocketPrivateImpl::TCPSocketPrivateImpl(PP_Instance instance,
                                           uint32 socket_id)
    : Resource(instance) {
  Init(socket_id);
}

TCPSocketPrivateImpl::TCPSocketPrivateImpl(const HostResource& resource,
                                           uint32 socket_id)
    : Resource(resource) {
  Init(socket_id);
}

TCPSocketPrivateImpl::~TCPSocketPrivateImpl() {
}

thunk::PPB_TCPSocket_Private_API*
TCPSocketPrivateImpl::AsPPB_TCPSocket_Private_API() {
  return this;
}

int32_t TCPSocketPrivateImpl::Connect(const char* host,
                                      uint16_t port,
                                      PP_CompletionCallback callback) {
  if (!host)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = new TrackedCallback(this, callback);
  // Send the request, the browser will call us back via ConnectACK.
  SendConnect(host, port);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketPrivateImpl::ConnectWithNetAddress(
    const PP_NetAddress_Private* addr,
    PP_CompletionCallback callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = new TrackedCallback(this, callback);
  // Send the request, the browser will call us back via ConnectACK.
  SendConnectWithNetAddress(*addr);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool TCPSocketPrivateImpl::GetLocalAddress(
    PP_NetAddress_Private* local_addr) {
  if (!IsConnected() || !local_addr)
    return PP_FALSE;

  *local_addr = local_addr_;
  return PP_TRUE;
}

PP_Bool TCPSocketPrivateImpl::GetRemoteAddress(
    PP_NetAddress_Private* remote_addr) {
  if (!IsConnected() || !remote_addr)
    return PP_FALSE;

  *remote_addr = remote_addr_;
  return PP_TRUE;
}

int32_t TCPSocketPrivateImpl::SSLHandshake(const char* server_name,
                                           uint16_t server_port,
                                           PP_CompletionCallback callback) {
  if (!server_name)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (connection_state_ != CONNECTED)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(ssl_handshake_callback_) ||
      TrackedCallback::IsPending(read_callback_) ||
      TrackedCallback::IsPending(write_callback_))
    return PP_ERROR_INPROGRESS;

  ssl_handshake_callback_ = new TrackedCallback(this, callback);

  // Send the request, the browser will call us back via SSLHandshakeACK.
  SendSSLHandshake(server_name, server_port);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketPrivateImpl::Read(char* buffer,
                                   int32_t bytes_to_read,
                                   PP_CompletionCallback callback) {
  if (!buffer || bytes_to_read <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(read_callback_) ||
      TrackedCallback::IsPending(ssl_handshake_callback_))
    return PP_ERROR_INPROGRESS;
  // TODO(dmichael): use some other strategy for determining if an
  // operation is in progress
  read_buffer_ = buffer;
  bytes_to_read_ = std::min(bytes_to_read, kMaxReadSize);
  read_callback_ = new TrackedCallback(this, callback);

  // Send the request, the browser will call us back via ReadACK.
  SendRead(bytes_to_read_);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketPrivateImpl::Write(const char* buffer,
                                    int32_t bytes_to_write,
                                    PP_CompletionCallback callback) {
  if (!buffer || bytes_to_write <= 0)
    return PP_ERROR_BADARGUMENT;
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(write_callback_) ||
      TrackedCallback::IsPending(ssl_handshake_callback_))
    return PP_ERROR_INPROGRESS;

  if (bytes_to_write > kMaxWriteSize)
    bytes_to_write = kMaxWriteSize;

  write_callback_ = new TrackedCallback(this, callback);

  // Send the request, the browser will call us back via WriteACK.
  SendWrite(std::string(buffer, bytes_to_write));
  return PP_OK_COMPLETIONPENDING;
}

void TCPSocketPrivateImpl::Disconnect() {
  if (connection_state_ == DISCONNECTED)
    return;

  connection_state_ = DISCONNECTED;

  SendDisconnect();
  socket_id_ = 0;

  PostAbortIfNecessary(&connect_callback_);
  PostAbortIfNecessary(&ssl_handshake_callback_);
  PostAbortIfNecessary(&read_callback_);
  PostAbortIfNecessary(&write_callback_);
  read_buffer_ = NULL;
  bytes_to_read_ = -1;
}

void TCPSocketPrivateImpl::OnConnectCompleted(
    bool succeeded,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  if (connection_state_ != BEFORE_CONNECT ||
      !TrackedCallback::IsPending(connect_callback_)) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    local_addr_ = local_addr;
    remote_addr_ = remote_addr;
    connection_state_ = CONNECTED;
  }
  TrackedCallback::ClearAndRun(&connect_callback_,
                               succeeded ? PP_OK : PP_ERROR_FAILED);
}

void TCPSocketPrivateImpl::OnSSLHandshakeCompleted(bool succeeded) {
  if (connection_state_ != CONNECTED ||
      !TrackedCallback::IsPending(ssl_handshake_callback_)) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    connection_state_ = SSL_CONNECTED;
    TrackedCallback::ClearAndRun(&ssl_handshake_callback_, PP_OK);
  } else {
    TrackedCallback::ClearAndRun(&ssl_handshake_callback_, PP_ERROR_FAILED);
    Disconnect();
  }
}

void TCPSocketPrivateImpl::OnReadCompleted(bool succeeded,
                                           const std::string& data) {
  if (!TrackedCallback::IsPending(read_callback_) || !read_buffer_) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memcpy(read_buffer_, data.c_str(), data.size());
  }
  read_buffer_ = NULL;
  bytes_to_read_ = -1;

  TrackedCallback::ClearAndRun(
      &read_callback_,
      succeeded ? static_cast<int32_t>(data.size()) :
                  static_cast<int32_t>(PP_ERROR_FAILED));
}

void TCPSocketPrivateImpl::OnWriteCompleted(bool succeeded,
                                            int32_t bytes_written) {
  if (!TrackedCallback::IsPending(write_callback_) ||
      (succeeded && bytes_written < 0)) {
    NOTREACHED();
    return;
  }

  TrackedCallback::ClearAndRun(
      &write_callback_,
      succeeded ? bytes_written : static_cast<int32_t>(PP_ERROR_FAILED));
}

void TCPSocketPrivateImpl::Init(uint32 socket_id) {
  DCHECK(socket_id != 0);
  socket_id_ = socket_id;
  connection_state_ = BEFORE_CONNECT;
  read_buffer_ = NULL;
  bytes_to_read_ = -1;

  local_addr_.size = 0;
  memset(local_addr_.data, 0,
         arraysize(local_addr_.data) * sizeof(*local_addr_.data));
  remote_addr_.size = 0;
  memset(remote_addr_.data, 0,
         arraysize(remote_addr_.data) * sizeof(*remote_addr_.data));
}

bool TCPSocketPrivateImpl::IsConnected() const {
  return connection_state_ == CONNECTED || connection_state_ == SSL_CONNECTED;
}

void TCPSocketPrivateImpl::PostAbortIfNecessary(
    scoped_refptr<TrackedCallback>* callback) {
  if (callback->get())
    (*callback)->PostAbort();
}

}  // namespace ppapi
