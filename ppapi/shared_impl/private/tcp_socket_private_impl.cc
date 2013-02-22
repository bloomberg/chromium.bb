// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_x509_certificate_private_api.h"

namespace ppapi {

const int32_t TCPSocketPrivateImpl::kMaxReadSize = 1024 * 1024;
const int32_t TCPSocketPrivateImpl::kMaxWriteSize = 1024 * 1024;

TCPSocketPrivateImpl::TCPSocketPrivateImpl(PP_Instance instance,
                                           uint32 socket_id)
    : Resource(OBJECT_IS_IMPL, instance),
      resource_type_(OBJECT_IS_IMPL) {
  Init(socket_id);
}

TCPSocketPrivateImpl::TCPSocketPrivateImpl(const HostResource& resource,
                                           uint32 socket_id)
    : Resource(OBJECT_IS_PROXY, resource),
      resource_type_(OBJECT_IS_PROXY) {
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
                                      scoped_refptr<TrackedCallback> callback) {
  if (!host)
    return PP_ERROR_BADARGUMENT;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = callback;
  // Send the request, the browser will call us back via ConnectACK.
  SendConnect(host, port);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketPrivateImpl::ConnectWithNetAddress(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = callback;
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

int32_t TCPSocketPrivateImpl::SSLHandshake(
    const char* server_name,
    uint16_t server_port,
    scoped_refptr<TrackedCallback> callback) {
  if (!server_name)
    return PP_ERROR_BADARGUMENT;

  if (connection_state_ != CONNECTED)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(ssl_handshake_callback_) ||
      TrackedCallback::IsPending(read_callback_) ||
      TrackedCallback::IsPending(write_callback_))
    return PP_ERROR_INPROGRESS;

  ssl_handshake_callback_ = callback;

  // Send the request, the browser will call us back via SSLHandshakeACK.
  SendSSLHandshake(server_name, server_port, trusted_certificates_,
      untrusted_certificates_);
  return PP_OK_COMPLETIONPENDING;
}

PP_Resource TCPSocketPrivateImpl::GetServerCertificate() {
  if (!server_certificate_.get())
    return 0;
  return server_certificate_->GetReference();
}

PP_Bool TCPSocketPrivateImpl::AddChainBuildingCertificate(
    PP_Resource certificate,
    PP_Bool trusted) {
  // TODO(raymes): The plumbing for this functionality is implemented but the
  // certificates aren't yet used for the connection, so just return false for
  // now.
  return PP_FALSE;

  thunk::EnterResourceNoLock<thunk::PPB_X509Certificate_Private_API>
  enter_cert(certificate, true);
  if (enter_cert.failed())
    return PP_FALSE;

  PP_Var der_var = enter_cert.object()->GetField(
      PP_X509CERTIFICATE_PRIVATE_RAW);
  ArrayBufferVar* der_array_buffer = ArrayBufferVar::FromPPVar(der_var);
  PP_Bool success = PP_FALSE;
  if (der_array_buffer) {
    const char* der_bytes = static_cast<const char*>(der_array_buffer->Map());
    uint32_t der_length = der_array_buffer->ByteLength();
    std::vector<char> der(der_bytes, der_bytes + der_length);
    if (PP_ToBool(trusted))
      trusted_certificates_.push_back(der);
    else
      untrusted_certificates_.push_back(der);
    success = PP_TRUE;
  }
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(der_var);
  return success;
}

int32_t TCPSocketPrivateImpl::Read(char* buffer,
                                   int32_t bytes_to_read,
                                   scoped_refptr<TrackedCallback> callback) {
  if (!buffer || bytes_to_read <= 0)
    return PP_ERROR_BADARGUMENT;

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(read_callback_) ||
      TrackedCallback::IsPending(ssl_handshake_callback_))
    return PP_ERROR_INPROGRESS;
  read_buffer_ = buffer;
  bytes_to_read_ = std::min(bytes_to_read, kMaxReadSize);
  read_callback_ = callback;

  // Send the request, the browser will call us back via ReadACK.
  SendRead(bytes_to_read_);
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketPrivateImpl::Write(const char* buffer,
                                    int32_t bytes_to_write,
                                    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || bytes_to_write <= 0)
    return PP_ERROR_BADARGUMENT;

  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(write_callback_) ||
      TrackedCallback::IsPending(ssl_handshake_callback_))
    return PP_ERROR_INPROGRESS;

  if (bytes_to_write > kMaxWriteSize)
    bytes_to_write = kMaxWriteSize;

  write_callback_ = callback;

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
  PostAbortIfNecessary(&set_option_callback_);
  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  server_certificate_ = NULL;
}

int32_t TCPSocketPrivateImpl::SetOption(
    PP_TCPSocketOption_Private name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  if (!IsConnected())
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(set_option_callback_))
    return PP_ERROR_INPROGRESS;

  set_option_callback_ = callback;

  switch (name) {
    case PP_TCPSOCKETOPTION_NO_DELAY:
      if (value.type != PP_VARTYPE_BOOL)
        return PP_ERROR_BADARGUMENT;
      SendSetBoolOption(name, PP_ToBool(value.value.as_bool));
      return PP_OK_COMPLETIONPENDING;
    default:
      return PP_ERROR_BADARGUMENT;
  }
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
  connect_callback_->Run(succeeded ? PP_OK : PP_ERROR_FAILED);
}

void TCPSocketPrivateImpl::OnSSLHandshakeCompleted(
    bool succeeded,
    const PPB_X509Certificate_Fields& certificate_fields) {
  if (connection_state_ != CONNECTED ||
      !TrackedCallback::IsPending(ssl_handshake_callback_)) {
    NOTREACHED();
    return;
  }

  if (succeeded) {
    connection_state_ = SSL_CONNECTED;
    server_certificate_ = new PPB_X509Certificate_Private_Shared(
        resource_type_,
        pp_instance(),
        certificate_fields);
    ssl_handshake_callback_->Run(PP_OK);
  } else {
    // The resource might be released in the callback so we need to hold
    // a reference so we can Disconnect() first.
    AddRef();
    ssl_handshake_callback_->Run(PP_ERROR_FAILED);
    Disconnect();
    Release();
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

  read_callback_->Run(
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

  write_callback_->Run(
      succeeded ? bytes_written : static_cast<int32_t>(PP_ERROR_FAILED));
}

void TCPSocketPrivateImpl::OnSetOptionCompleted(bool succeeded) {
  if (!TrackedCallback::IsPending(set_option_callback_)) {
    NOTREACHED();
    return;
  }

  set_option_callback_->Run(succeeded ? PP_OK : PP_ERROR_FAILED);
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
  if (TrackedCallback::IsPending(*callback))
    (*callback)->PostAbort();
}

}  // namespace ppapi
