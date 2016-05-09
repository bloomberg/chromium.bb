// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/interface_registry.h"

#include "services/shell/public/cpp/connection.h"

namespace shell {

InterfaceRegistry::InterfaceRegistry(Connection* connection)
    : InterfaceRegistry(nullptr, connection) {}

InterfaceRegistry::InterfaceRegistry(mojom::InterfaceProviderRequest request,
                                     Connection* connection)
    : binding_(this), connection_(connection) {
  if (!request.is_pending())
    request = GetProxy(&client_handle_);
  binding_.Bind(std::move(request));
}

InterfaceRegistry::~InterfaceRegistry() {}

mojom::InterfaceProviderPtr InterfaceRegistry::TakeClientHandle() {
  return std::move(client_handle_);
}

// mojom::InterfaceProvider:
void InterfaceRegistry::GetInterface(const mojo::String& interface_name,
                                     mojo::ScopedMessagePipeHandle handle) {
  auto iter = name_to_binder_.find(interface_name);
  if (iter != name_to_binder_.end()) {
    iter->second->BindInterface(connection_, interface_name, std::move(handle));
  } else if (connection_ && connection_->AllowsInterface(interface_name)) {
    LOG(ERROR) << "Connection CapabilityFilter prevented binding to "
               << "interface: " << interface_name << " connection_name:"
               << connection_->GetConnectionName() << " remote_name:"
               << connection_->GetRemoteIdentity().name();
  }
}

bool InterfaceRegistry::SetInterfaceBinderForName(
    std::unique_ptr<InterfaceBinder> binder,
    const std::string& interface_name) {
  if (!connection_ ||
      (connection_ && connection_->AllowsInterface(interface_name))) {
    RemoveInterfaceBinderForName(interface_name);
    name_to_binder_[interface_name] = std::move(binder);
    return true;
  }
  return false;
}

void InterfaceRegistry::RemoveInterfaceBinderForName(
    const std::string& interface_name) {
  NameToInterfaceBinderMap::iterator it = name_to_binder_.find(interface_name);
  if (it == name_to_binder_.end())
    return;
  name_to_binder_.erase(it);
}

}  // namespace shell
