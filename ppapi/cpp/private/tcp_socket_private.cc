// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/tcp_socket_private.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TCPSocket_Private>() {
  return PPB_TCPSOCKET_PRIVATE_INTERFACE;
}

}  // namespace

TCPSocketPrivate::TCPSocketPrivate(Instance* instance) {
  if (has_interface<PPB_TCPSocket_Private>() && instance) {
    PassRefFromConstructor(get_interface<PPB_TCPSocket_Private>()->Create(
        instance->pp_instance()));
  }
}

// static
bool TCPSocketPrivate::IsAvailable() {
  return has_interface<PPB_TCPSocket_Private>();
}

int32_t TCPSocketPrivate::Connect(const char* host,
                                  uint16_t port,
                                  const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPSocket_Private>()->Connect(
      pp_resource(), host, port, callback.pp_completion_callback());
}

int32_t TCPSocketPrivate::ConnectWithNetAddress(
    const PP_NetAddress_Private* addr,
    const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPSocket_Private>()->ConnectWithNetAddress(
      pp_resource(), addr, callback.pp_completion_callback());
}

bool TCPSocketPrivate::GetLocalAddress(PP_NetAddress_Private* local_addr) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return false;

  PP_Bool result = get_interface<PPB_TCPSocket_Private>()->GetLocalAddress(
      pp_resource(), local_addr);
  return PP_ToBool(result);
}

bool TCPSocketPrivate::GetRemoteAddress(PP_NetAddress_Private* remote_addr) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return false;
  PP_Bool result = get_interface<PPB_TCPSocket_Private>()->GetRemoteAddress(
      pp_resource(), remote_addr);
  return PP_ToBool(result);
}

int32_t TCPSocketPrivate::SSLHandshake(const char* server_name,
                                       uint16_t server_port,
                                       const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPSocket_Private>()->SSLHandshake(
      pp_resource(), server_name, server_port,
      callback.pp_completion_callback());
}

int32_t TCPSocketPrivate::Read(char* buffer,
                               int32_t bytes_to_read,
                               const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPSocket_Private>()->Read(
      pp_resource(), buffer, bytes_to_read, callback.pp_completion_callback());
}

int32_t TCPSocketPrivate::Write(const char* buffer,
                                int32_t bytes_to_write,
                                const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPSocket_Private>()->Write(
      pp_resource(), buffer, bytes_to_write, callback.pp_completion_callback());
}

void TCPSocketPrivate::Disconnect() {
  if (!has_interface<PPB_TCPSocket_Private>())
    return;
  return get_interface<PPB_TCPSocket_Private>()->Disconnect(pp_resource());
}

}  // namespace pp
