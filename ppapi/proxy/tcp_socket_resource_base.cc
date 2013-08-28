// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/tcp_socket_resource_base.h"

#include <cstring>

#include "base/bind.h"
#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/error_conversion.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/private/ppb_x509_certificate_private_shared.h"
#include "ppapi/shared_impl/socket_option_data.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_x509_certificate_private_api.h"

namespace ppapi {
namespace proxy {

const int32_t TCPSocketResourceBase::kMaxReadSize = 1024 * 1024;
const int32_t TCPSocketResourceBase::kMaxWriteSize = 1024 * 1024;
const int32_t TCPSocketResourceBase::kMaxSendBufferSize =
    1024 * TCPSocketResourceBase::kMaxWriteSize;
const int32_t TCPSocketResourceBase::kMaxReceiveBufferSize =
    1024 * TCPSocketResourceBase::kMaxReadSize;

TCPSocketResourceBase::TCPSocketResourceBase(Connection connection,
                                             PP_Instance instance,
                                             bool private_api)
    : PluginResource(connection, instance),
      connection_state_(BEFORE_CONNECT),
      read_buffer_(NULL),
      bytes_to_read_(-1),
      private_api_(private_api) {
  local_addr_.size = 0;
  memset(local_addr_.data, 0,
         arraysize(local_addr_.data) * sizeof(*local_addr_.data));
  remote_addr_.size = 0;
  memset(remote_addr_.data, 0,
         arraysize(remote_addr_.data) * sizeof(*remote_addr_.data));
}

TCPSocketResourceBase::TCPSocketResourceBase(
    Connection connection,
    PP_Instance instance,
    bool private_api,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr)
    : PluginResource(connection, instance),
      connection_state_(CONNECTED),
      read_buffer_(NULL),
      bytes_to_read_(-1),
      local_addr_(local_addr),
      remote_addr_(remote_addr),
      private_api_(private_api) {
}

TCPSocketResourceBase::~TCPSocketResourceBase() {
}

int32_t TCPSocketResourceBase::ConnectImpl(
    const char* host,
    uint16_t port,
    scoped_refptr<TrackedCallback> callback) {
  if (!host)
    return PP_ERROR_BADARGUMENT;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = callback;

  Call<PpapiPluginMsg_TCPSocket_ConnectReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Connect(host, port),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgConnectReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::ConnectWithNetAddressImpl(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  if (!addr)
    return PP_ERROR_BADARGUMENT;
  if (connection_state_ != BEFORE_CONNECT)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one pending request.

  connect_callback_ = callback;

  Call<PpapiPluginMsg_TCPSocket_ConnectReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_ConnectWithNetAddress(*addr),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgConnectReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool TCPSocketResourceBase::GetLocalAddressImpl(
    PP_NetAddress_Private* local_addr) {
  if (!IsConnected() || !local_addr)
    return PP_FALSE;
  *local_addr = local_addr_;
  return PP_TRUE;
}

PP_Bool TCPSocketResourceBase::GetRemoteAddressImpl(
    PP_NetAddress_Private* remote_addr) {
  if (!IsConnected() || !remote_addr)
    return PP_FALSE;
  *remote_addr = remote_addr_;
  return PP_TRUE;
}

int32_t TCPSocketResourceBase::SSLHandshakeImpl(
    const char* server_name,
    uint16_t server_port,
    scoped_refptr<TrackedCallback> callback) {
  if (!server_name)
    return PP_ERROR_BADARGUMENT;

  if (connection_state_ != CONNECTED)
    return PP_ERROR_FAILED;
  if (TrackedCallback::IsPending(ssl_handshake_callback_) ||
      TrackedCallback::IsPending(read_callback_) ||
      TrackedCallback::IsPending(write_callback_)) {
    return PP_ERROR_INPROGRESS;
  }

  ssl_handshake_callback_ = callback;

  Call<PpapiPluginMsg_TCPSocket_SSLHandshakeReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_SSLHandshake(server_name,
                                          server_port,
                                          trusted_certificates_,
                                          untrusted_certificates_),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgSSLHandshakeReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

PP_Resource TCPSocketResourceBase::GetServerCertificateImpl() {
  if (!server_certificate_.get())
    return 0;
  return server_certificate_->GetReference();
}

PP_Bool TCPSocketResourceBase::AddChainBuildingCertificateImpl(
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

int32_t TCPSocketResourceBase::ReadImpl(
    char* buffer,
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

  Call<PpapiPluginMsg_TCPSocket_ReadReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Read(bytes_to_read_),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgReadReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t TCPSocketResourceBase::WriteImpl(
    const char* buffer,
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

  Call<PpapiPluginMsg_TCPSocket_WriteReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_Write(std::string(buffer, bytes_to_write)),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgWriteReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

void TCPSocketResourceBase::DisconnectImpl() {
  if (connection_state_ == DISCONNECTED)
    return;

  connection_state_ = DISCONNECTED;

  Post(BROWSER, PpapiHostMsg_TCPSocket_Disconnect());

  PostAbortIfNecessary(&connect_callback_);
  PostAbortIfNecessary(&ssl_handshake_callback_);
  PostAbortIfNecessary(&read_callback_);
  PostAbortIfNecessary(&write_callback_);
  read_buffer_ = NULL;
  bytes_to_read_ = -1;
  server_certificate_ = NULL;
}

int32_t TCPSocketResourceBase::SetOptionImpl(
    PP_TCPSocket_Option name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  if (!IsConnected())
    return PP_ERROR_FAILED;

  SocketOptionData option_data;
  switch (name) {
    case PP_TCPSOCKET_OPTION_NO_DELAY: {
      if (value.type != PP_VARTYPE_BOOL)
        return PP_ERROR_BADARGUMENT;
      option_data.SetBool(PP_ToBool(value.value.as_bool));
      break;
    }
    case PP_TCPSOCKET_OPTION_SEND_BUFFER_SIZE:
    case PP_TCPSOCKET_OPTION_RECV_BUFFER_SIZE: {
      if (value.type != PP_VARTYPE_INT32)
        return PP_ERROR_BADARGUMENT;
      option_data.SetInt32(value.value.as_int);
      break;
    }
    default: {
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
    }
  }

  set_option_callbacks_.push(callback);

  Call<PpapiPluginMsg_TCPSocket_SetOptionReply>(
      BROWSER,
      PpapiHostMsg_TCPSocket_SetOption(name, option_data),
      base::Bind(&TCPSocketResourceBase::OnPluginMsgSetOptionReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

bool TCPSocketResourceBase::IsConnected() const {
  return connection_state_ == CONNECTED || connection_state_ == SSL_CONNECTED;
}

void TCPSocketResourceBase::PostAbortIfNecessary(
    scoped_refptr<TrackedCallback>* callback) {
  if (TrackedCallback::IsPending(*callback))
    (*callback)->PostAbort();
}

void TCPSocketResourceBase::OnPluginMsgConnectReply(
    const ResourceMessageReplyParams& params,
    const PP_NetAddress_Private& local_addr,
    const PP_NetAddress_Private& remote_addr) {
  // It is possible that |connect_callback_| is pending while
  // |connection_state_| is not BEFORE_CONNECT: DisconnectImpl() has been
  // called, but a ConnectCompleted notification came earlier than the task to
  // abort |connect_callback_|. We don't want to update |connection_state_| or
  // other members in that case.
  if (connection_state_ != BEFORE_CONNECT ||
      !TrackedCallback::IsPending(connect_callback_)) {
    return;
  }

  if (params.result() == PP_OK) {
    local_addr_ = local_addr;
    remote_addr_ = remote_addr;
    connection_state_ = CONNECTED;
  }
  RunCallback(connect_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgSSLHandshakeReply(
      const ResourceMessageReplyParams& params,
      const PPB_X509Certificate_Fields& certificate_fields) {
  // It is possible that |ssl_handshake_callback_| is pending while
  // |connection_state_| is not CONNECT: DisconnectImpl() has been
  // called, but a SSLHandshakeCompleted notification came earlier than the task
  // to abort |ssl_handshake_callback_|. We don't want to update
  // |connection_state_| or other members in that case.
  if (connection_state_ != CONNECTED ||
      !TrackedCallback::IsPending(ssl_handshake_callback_)) {
    return;
  }

  if (params.result() == PP_OK) {
    connection_state_ = SSL_CONNECTED;
    server_certificate_ = new PPB_X509Certificate_Private_Shared(
        OBJECT_IS_PROXY,
        pp_instance(),
        certificate_fields);
    RunCallback(ssl_handshake_callback_, params.result());
  } else {
    // The resource might be released in the callback so we need to hold
    // a reference so we can Disconnect() first.
    AddRef();
    RunCallback(ssl_handshake_callback_, params.result());
    DisconnectImpl();
    Release();
  }
}

void TCPSocketResourceBase::OnPluginMsgReadReply(
    const ResourceMessageReplyParams& params,
    const std::string& data) {
  // It is possible that |read_callback_| is pending while |read_buffer_| is
  // NULL: DisconnectImpl() has been called, but a ReadCompleted notification
  // came earlier than the task to abort |read_callback_|. We shouldn't access
  // the buffer in that case. The user may have released it.
  if (!TrackedCallback::IsPending(read_callback_) || !read_buffer_)
    return;

  const bool succeeded = params.result() == PP_OK;
  if (succeeded) {
    CHECK_LE(static_cast<int32_t>(data.size()), bytes_to_read_);
    if (!data.empty())
      memmove(read_buffer_, data.c_str(), data.size());
  }
  read_buffer_ = NULL;
  bytes_to_read_ = -1;

  read_callback_->Run(succeeded ?
                      static_cast<int32_t>(data.size()) :
                      ConvertNetworkAPIErrorForCompatibility(params.result(),
                                                             private_api_));
}

void TCPSocketResourceBase::OnPluginMsgWriteReply(
    const ResourceMessageReplyParams& params) {
  if (!TrackedCallback::IsPending(write_callback_))
    return;
  RunCallback(write_callback_, params.result());
}

void TCPSocketResourceBase::OnPluginMsgSetOptionReply(
    const ResourceMessageReplyParams& params) {
  if (set_option_callbacks_.empty()) {
    NOTREACHED();
    return;
  }
  scoped_refptr<TrackedCallback> callback = set_option_callbacks_.front();
  set_option_callbacks_.pop();
  if (TrackedCallback::IsPending(callback))
    RunCallback(callback, params.result());
}

void TCPSocketResourceBase::RunCallback(scoped_refptr<TrackedCallback> callback,
                                        int32_t pp_result) {
  callback->Run(ConvertNetworkAPIErrorForCompatibility(pp_result,
                                                       private_api_));
}

}  // namespace ppapi
}  // namespace proxy
