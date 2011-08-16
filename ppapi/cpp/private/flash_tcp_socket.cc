// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(yzshen): See the comment in corresponding .h file.

#include "ppapi/cpp/private/flash_tcp_socket.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash_TCPSocket>() {
  return PPB_FLASH_TCPSOCKET_INTERFACE;
}

}  // namespace

namespace flash {

TCPSocket::TCPSocket(Instance* instance) {
  if (has_interface<PPB_Flash_TCPSocket>() && instance) {
    PassRefFromConstructor(get_interface<PPB_Flash_TCPSocket>()->Create(
        instance->pp_instance()));
  }
}

int32_t TCPSocket::Connect(const char* host,
                           uint16_t port,
                           const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_TCPSocket>()->Connect(
      pp_resource(), host, port, callback.pp_completion_callback());
}

int32_t TCPSocket::ConnectWithNetAddress(const PP_Flash_NetAddress* addr,
                                         const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_TCPSocket>()->ConnectWithNetAddress(
      pp_resource(), addr, callback.pp_completion_callback());
}

bool TCPSocket::GetLocalAddress(PP_Flash_NetAddress* local_addr) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return false;

  PP_Bool result = get_interface<PPB_Flash_TCPSocket>()->GetLocalAddress(
      pp_resource(), local_addr);
  return PP_ToBool(result);
}

bool TCPSocket::GetRemoteAddress(PP_Flash_NetAddress* remote_addr) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return false;
  PP_Bool result = get_interface<PPB_Flash_TCPSocket>()->GetRemoteAddress(
      pp_resource(), remote_addr);
  return PP_ToBool(result);
}

int32_t TCPSocket::SSLHandshake(const char* server_name,
                                uint16_t server_port,
                                const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_TCPSocket>()->SSLHandshake(
      pp_resource(), server_name, server_port,
      callback.pp_completion_callback());
}

int32_t TCPSocket::Read(char* buffer,
                        int32_t bytes_to_read,
                        const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_TCPSocket>()->Read(
      pp_resource(), buffer, bytes_to_read, callback.pp_completion_callback());
}

int32_t TCPSocket::Write(const char* buffer,
                         int32_t bytes_to_write,
                         const CompletionCallback& callback) {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_Flash_TCPSocket>()->Write(
      pp_resource(), buffer, bytes_to_write, callback.pp_completion_callback());
}

void TCPSocket::Disconnect() {
  if (!has_interface<PPB_Flash_TCPSocket>())
    return;
  return get_interface<PPB_Flash_TCPSocket>()->Disconnect(pp_resource());
}

}  // namespace flash
}  // namespace pp
