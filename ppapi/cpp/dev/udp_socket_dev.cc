// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/udp_socket_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_UDPSocket_Dev_0_1>() {
  return PPB_UDPSOCKET_DEV_INTERFACE_0_1;
}

}  // namespace

UDPSocket_Dev::UDPSocket_Dev() {
}

UDPSocket_Dev::UDPSocket_Dev(const InstanceHandle& instance) {
  if (has_interface<PPB_UDPSocket_Dev_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_UDPSocket_Dev_0_1>()->Create(
        instance.pp_instance()));
  }
}

UDPSocket_Dev::UDPSocket_Dev(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

UDPSocket_Dev::UDPSocket_Dev(const UDPSocket_Dev& other) : Resource(other) {
}

UDPSocket_Dev::~UDPSocket_Dev() {
}

UDPSocket_Dev& UDPSocket_Dev::operator=(const UDPSocket_Dev& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool UDPSocket_Dev::IsAvailable() {
  return has_interface<PPB_UDPSocket_Dev_0_1>();
}

int32_t UDPSocket_Dev::Bind(const NetAddress_Dev& addr,
                            const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_Dev_0_1>()) {
    return get_interface<PPB_UDPSocket_Dev_0_1>()->Bind(
        pp_resource(), addr.pp_resource(), callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

NetAddress_Dev UDPSocket_Dev::GetBoundAddress() {
  if (has_interface<PPB_UDPSocket_Dev_0_1>()) {
    return NetAddress_Dev(
        PASS_REF,
        get_interface<PPB_UDPSocket_Dev_0_1>()->GetBoundAddress(pp_resource()));
  }
  return NetAddress_Dev();
}

int32_t UDPSocket_Dev::RecvFrom(
    char* buffer,
    int32_t num_bytes,
    const CompletionCallbackWithOutput<NetAddress_Dev>& callback) {
  if (has_interface<PPB_UDPSocket_Dev_0_1>()) {
    return get_interface<PPB_UDPSocket_Dev_0_1>()->RecvFrom(
        pp_resource(), buffer, num_bytes, callback.output(),
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t UDPSocket_Dev::SendTo(const char* buffer,
                              int32_t num_bytes,
                              const NetAddress_Dev& addr,
                              const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_Dev_0_1>()) {
    return get_interface<PPB_UDPSocket_Dev_0_1>()->SendTo(
        pp_resource(), buffer, num_bytes, addr.pp_resource(),
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

void UDPSocket_Dev::Close() {
  if (has_interface<PPB_UDPSocket_Dev_0_1>())
    return get_interface<PPB_UDPSocket_Dev_0_1>()->Close(pp_resource());
}

int32_t UDPSocket_Dev::SetOption(PP_UDPSocket_Option_Dev name,
                                 const Var& value,
                                 const CompletionCallback& callback) {
  if (has_interface<PPB_UDPSocket_Dev_0_1>()) {
    return get_interface<PPB_UDPSocket_Dev_0_1>()->SetOption(
        pp_resource(), name, value.pp_var(),
        callback.pp_completion_callback());
  }
  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

}  // namespace pp
