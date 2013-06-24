// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/private/tcp_socket_private_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"

namespace ppapi {

TCPSocketPrivateImpl::TCPSocketPrivateImpl(PP_Instance instance,
                                           uint32 socket_id)
    : Resource(OBJECT_IS_IMPL, instance),
      TCPSocketShared(OBJECT_IS_IMPL, socket_id) {
}

TCPSocketPrivateImpl::TCPSocketPrivateImpl(const HostResource& resource,
                                           uint32 socket_id)
    : Resource(OBJECT_IS_PROXY, resource),
      TCPSocketShared(OBJECT_IS_PROXY, socket_id) {
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
  return ConnectImpl(host, port, callback);
}

int32_t TCPSocketPrivateImpl::ConnectWithNetAddress(
    const PP_NetAddress_Private* addr,
    scoped_refptr<TrackedCallback> callback) {
  return ConnectWithNetAddressImpl(addr, callback);
}

PP_Bool TCPSocketPrivateImpl::GetLocalAddress(
    PP_NetAddress_Private* local_addr) {
  return GetLocalAddressImpl(local_addr);
}

PP_Bool TCPSocketPrivateImpl::GetRemoteAddress(
    PP_NetAddress_Private* remote_addr) {
  return GetRemoteAddressImpl(remote_addr);
}

int32_t TCPSocketPrivateImpl::SSLHandshake(
    const char* server_name,
    uint16_t server_port,
    scoped_refptr<TrackedCallback> callback) {
  return SSLHandshakeImpl(server_name, server_port, callback);
}

PP_Resource TCPSocketPrivateImpl::GetServerCertificate() {
  return GetServerCertificateImpl();
}

PP_Bool TCPSocketPrivateImpl::AddChainBuildingCertificate(
    PP_Resource certificate,
    PP_Bool trusted) {
  return AddChainBuildingCertificateImpl(certificate, trusted);
}

int32_t TCPSocketPrivateImpl::Read(char* buffer,
                                   int32_t bytes_to_read,
                                   scoped_refptr<TrackedCallback> callback) {
  return ReadImpl(buffer, bytes_to_read, callback);
}

int32_t TCPSocketPrivateImpl::Write(const char* buffer,
                                    int32_t bytes_to_write,
                                    scoped_refptr<TrackedCallback> callback) {
  return WriteImpl(buffer, bytes_to_write, callback);
}

void TCPSocketPrivateImpl::Disconnect() {
  DisconnectImpl();
}

int32_t TCPSocketPrivateImpl::SetOption(
    PP_TCPSocketOption_Private name,
    const PP_Var& value,
    scoped_refptr<TrackedCallback> callback) {
  switch (name) {
    case PP_TCPSOCKETOPTION_PRIVATE_INVALID:
      return PP_ERROR_BADARGUMENT;
    case PP_TCPSOCKETOPTION_PRIVATE_NO_DELAY:
      return SetOptionImpl(PP_TCPSOCKET_OPTION_NO_DELAY, value, callback);
    default:
      NOTREACHED();
      return PP_ERROR_BADARGUMENT;
  }
}

Resource* TCPSocketPrivateImpl::GetOwnerResource() {
  return this;
}

int32_t TCPSocketPrivateImpl::OverridePPError(int32_t pp_error) {
  // PPB_TCPSocket_Private treats all errors from the browser process as
  // PP_ERROR_FAILED.
  if (pp_error < 0)
    return PP_ERROR_FAILED;

  return pp_error;
}

}  // namespace ppapi
