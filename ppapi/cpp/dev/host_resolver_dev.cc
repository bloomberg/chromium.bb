// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/host_resolver_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_HostResolver_Dev_0_1>() {
  return PPB_HOSTRESOLVER_DEV_INTERFACE_0_1;
}

}  // namespace

HostResolver_Dev::HostResolver_Dev() {
}

HostResolver_Dev::HostResolver_Dev(const InstanceHandle& instance) {
  if (has_interface<PPB_HostResolver_Dev_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_HostResolver_Dev_0_1>()->Create(
        instance.pp_instance()));
  }
}

HostResolver_Dev::HostResolver_Dev(PassRef, PP_Resource resource)
    : Resource(PASS_REF, resource) {
}

HostResolver_Dev::HostResolver_Dev(const HostResolver_Dev& other)
    : Resource(other) {
}

HostResolver_Dev::~HostResolver_Dev() {
}

HostResolver_Dev& HostResolver_Dev::operator=(const HostResolver_Dev& other) {
  Resource::operator=(other);
  return *this;
}

// static
bool HostResolver_Dev::IsAvailable() {
  return has_interface<PPB_HostResolver_Dev_0_1>();
}

int32_t HostResolver_Dev::Resolve(const char* host,
                                  uint16_t port,
                                  const PP_HostResolver_Hint_Dev& hint,
                                  const CompletionCallback& callback) {
  if (has_interface<PPB_HostResolver_Dev_0_1>()) {
    return get_interface<PPB_HostResolver_Dev_0_1>()->Resolve(
        pp_resource(), host, port, &hint, callback.pp_completion_callback());
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

Var HostResolver_Dev::GetCanonicalName() const {
  if (has_interface<PPB_HostResolver_Dev_0_1>()) {
    return Var(PASS_REF,
               get_interface<PPB_HostResolver_Dev_0_1>()->GetCanonicalName(
                   pp_resource()));
  }

  return Var();
}

uint32_t HostResolver_Dev::GetNetAddressCount() const {
  if (has_interface<PPB_HostResolver_Dev_0_1>()) {
    return get_interface<PPB_HostResolver_Dev_0_1>()->GetNetAddressCount(
        pp_resource());
  }

  return 0;
}

NetAddress_Dev HostResolver_Dev::GetNetAddress(uint32_t index) const {
  if (has_interface<PPB_HostResolver_Dev_0_1>()) {
    return NetAddress_Dev(
        PASS_REF,
        get_interface<PPB_HostResolver_Dev_0_1>()->GetNetAddress(
            pp_resource(), index));
  }

  return NetAddress_Dev();
}

}  // namespace pp
