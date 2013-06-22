// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/tcp_socket_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TCPSocket_Dev_0_1>() {
  return PPB_TCPSOCKET_DEV_INTERFACE_0_1;
}

}  // namespace

TCPSocket_Dev::TCPSocket_Dev() {
}

TCPSocket_Dev::TCPSocket_Dev(const InstanceHandle& instance) {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_TCPSocket_Dev_0_1>()->Create(
        instance.pp_instance()));
  }
}

TCPSocket_Dev::TCPSocket_Dev(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

TCPSocket_Dev::TCPSocket_Dev(const TCPSocket_Dev& other) : Resource(other) {
}

TCPSocket_Dev::~TCPSocket_Dev() {
}

TCPSocket_Dev& TCPSocket_Dev::operator=(const TCPSocket_Dev& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool TCPSocket_Dev::IsAvailable() {
  return has_interface<PPB_TCPSocket_Dev_0_1>();
}

int32_t TCPSocket_Dev::Connect(const NetAddress& addr,
                               const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    return get_interface<PPB_TCPSocket_Dev_0_1>()->Connect(
        pp_resource(), addr.pp_resource(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

NetAddress TCPSocket_Dev::GetLocalAddress() const {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_TCPSocket_Dev_0_1>()->GetLocalAddress(pp_resource()));
  }
  return NetAddress();
}

NetAddress TCPSocket_Dev::GetRemoteAddress() const {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    return NetAddress(
        PASS_REF,
        get_interface<PPB_TCPSocket_Dev_0_1>()->GetRemoteAddress(
            pp_resource()));
  }
  return NetAddress();
}

int32_t TCPSocket_Dev::Read(char* buffer,
                            int32_t bytes_to_read,
                            const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    return get_interface<PPB_TCPSocket_Dev_0_1>()->Read(
        pp_resource(), buffer, bytes_to_read,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t TCPSocket_Dev::Write(const char* buffer,
                             int32_t bytes_to_write,
                             const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    return get_interface<PPB_TCPSocket_Dev_0_1>()->Write(
        pp_resource(), buffer, bytes_to_write,
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

void TCPSocket_Dev::Close() {
  if (has_interface<PPB_TCPSocket_Dev_0_1>())
    get_interface<PPB_TCPSocket_Dev_0_1>()->Close(pp_resource());
}

int32_t TCPSocket_Dev::SetOption(PP_TCPSocket_Option_Dev name,
                                 const Var& value,
                                 const CompletionCallback& callback) {
  if (has_interface<PPB_TCPSocket_Dev_0_1>()) {
    return get_interface<PPB_TCPSocket_Dev_0_1>()->SetOption(
        pp_resource(), name, value.pp_var(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
