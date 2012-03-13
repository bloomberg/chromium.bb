// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/tcp_server_socket_private.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TCPServerSocket_Private>() {
  return PPB_TCPSERVERSOCKET_PRIVATE_INTERFACE;
}

}  // namespace

TCPServerSocketPrivate::TCPServerSocketPrivate(const InstanceHandle& instance) {
  if (has_interface<PPB_TCPServerSocket_Private>()) {
    PassRefFromConstructor(get_interface<PPB_TCPServerSocket_Private>()->Create(
        instance.pp_instance()));
  }
}

// static
bool TCPServerSocketPrivate::IsAvailable() {
  return has_interface<PPB_TCPServerSocket_Private>();
}

int32_t TCPServerSocketPrivate::Listen(const PP_NetAddress_Private* addr,
                                       int32_t backlog,
                                       const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPServerSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPServerSocket_Private>()->Listen(
      pp_resource(), addr, backlog, callback.pp_completion_callback());
}

int32_t TCPServerSocketPrivate::Accept(PP_Resource* tcp_socket,
                                       const CompletionCallback& callback) {
  if (!has_interface<PPB_TCPServerSocket_Private>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_TCPServerSocket_Private>()->Accept(
      pp_resource(), tcp_socket, callback.pp_completion_callback());
}

void TCPServerSocketPrivate::StopListening() {
  if (!has_interface<PPB_TCPServerSocket_Private>())
    return;
  return get_interface<PPB_TCPServerSocket_Private>()->StopListening(
      pp_resource());
}

}  // namespace pp
