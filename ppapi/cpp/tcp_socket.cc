// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/tcp_socket.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TCPSocket_1_0>() {
  return PPB_TCPSOCKET_INTERFACE_1_0;
}

}  // namespace

TCPSocket::TCPSocket() {
}

TCPSocket::TCPSocket(const InstanceHandle& instance) {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_TCPSocket_1_0>()->Create(
        instance.pp_instance()));
  }
}

TCPSocket::TCPSocket(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

TCPSocket::TCPSocket(const TCPSocket& other) : Resource(other) {
}

TCPSocket::~TCPSocket() {
}

TCPSocket& TCPSocket::operator=(const TCPSocket& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool TCPSocket::IsAvailable() {
  return has_interface<PPB_TCPSocket_1_0>();
}

int32_t TCPSocket::Connect(const NetAddress& addr,
                           const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    return get_interface<PPB_TCPSocket_1_0>()->Connect(
        pp_resource(), addr.pp_resource(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

NetAddress TCPSocket::GetLocalAddress() const {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_TCPSocket_1_0>()->GetLocalAddress(pp_resource()));
  }
  return NetAddress();
}

NetAddress TCPSocket::GetRemoteAddress() const {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_TCPSocket_1_0>()->GetRemoteAddress(pp_resource()));
  }
  return NetAddress();
}

int32_t TCPSocket::Read(char* buffer,
                        int32_t bytes_to_read,
                        const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    return get_interface<PPB_TCPSocket_1_0>()->Read(
        pp_resource(), buffer, bytes_to_read,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t TCPSocket::Write(const char* buffer,
                         int32_t bytes_to_write,
                         const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    return get_interface<PPB_TCPSocket_1_0>()->Write(
        pp_resource(), buffer, bytes_to_write,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

void TCPSocket::Close() {
  if (has_interface<PPB_TCPSocket_1_0>())
    get_interface<PPB_TCPSocket_1_0>()->Close(pp_resource());
}

int32_t TCPSocket::SetOption(PP_TCPSocket_Option name,
                             const Var& value,
                             const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_1_0>()) {
    return get_interface<PPB_TCPSocket_1_0>()->SetOption(
        pp_resource(), name, value.pp_var(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
