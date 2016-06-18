// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/remote_interface_registry.h"

namespace shell {

RemoteInterfaceRegistry::RemoteInterfaceRegistry(
    mojom::InterfaceProviderPtr remote_interfaces)
    : remote_interfaces_(std::move(remote_interfaces)) {}
RemoteInterfaceRegistry::~RemoteInterfaceRegistry() {}

mojom::InterfaceProvider* RemoteInterfaceRegistry::GetInterfaceProvider() {
  return remote_interfaces_.get();
}

void RemoteInterfaceRegistry::SetConnectionLostClosure(
    const base::Closure& connection_lost_closure) {
  remote_interfaces_.set_connection_error_handler(connection_lost_closure);
}

void RemoteInterfaceRegistry::GetInterface(
    const std::string& name,
    mojo::ScopedMessagePipeHandle request_handle) {
  remote_interfaces_->GetInterface(name, std::move(request_handle));
}

void RemoteInterfaceRegistry::ClearBinders() {
  binders_.clear();
}

}  // namespace shell
