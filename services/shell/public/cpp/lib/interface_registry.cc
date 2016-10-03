// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shell/public/cpp/interface_registry.h"

#include <sstream>

#include "mojo/public/cpp/bindings/message.h"
#include "services/shell/public/cpp/connection.h"

namespace shell {

InterfaceRegistry::InterfaceRegistry()
    : binding_(this), allow_all_interfaces_(true), weak_factory_(this) {}

InterfaceRegistry::InterfaceRegistry(
    const Identity& remote_identity,
    const CapabilityRequest& capability_request)
    : binding_(this),
      remote_identity_(remote_identity),
      capability_request_(capability_request),
      allow_all_interfaces_(capability_request.interfaces.size() == 1 &&
                            capability_request.interfaces.count("*") == 1),
      weak_factory_(this) {}

InterfaceRegistry::~InterfaceRegistry() {}

void InterfaceRegistry::Bind(
    mojom::InterfaceProviderRequest local_interfaces_request) {
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(local_interfaces_request));
}

base::WeakPtr<InterfaceRegistry> InterfaceRegistry::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool InterfaceRegistry::AddInterface(
    const std::string& name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  return SetInterfaceBinderForName(
      base::MakeUnique<internal::GenericCallbackBinder>(callback, task_runner),
      name);
}

void InterfaceRegistry::RemoveInterface(const std::string& name) {
  auto it = name_to_binder_.find(name);
  if (it != name_to_binder_.end())
    name_to_binder_.erase(it);
}

void InterfaceRegistry::PauseBinding() {
  DCHECK(!is_paused_);
  is_paused_ = true;
}

void InterfaceRegistry::ResumeBinding() {
  DCHECK(is_paused_);
  is_paused_ = false;

  while (!pending_interface_requests_.empty()) {
    auto& request = pending_interface_requests_.front();
    GetInterface(request.first, std::move(request.second));
    pending_interface_requests_.pop();
  }
}

void InterfaceRegistry::GetInterfaceNames(
    std::set<std::string>* interface_names) {
  DCHECK(interface_names);
  for (auto& entry : name_to_binder_)
    interface_names->insert(entry.first);
}

void InterfaceRegistry::SetConnectionLostClosure(
    const base::Closure& connection_lost_closure) {
  binding_.set_connection_error_handler(connection_lost_closure);
}

// mojom::InterfaceProvider:
void InterfaceRegistry::GetInterface(const std::string& interface_name,
                                     mojo::ScopedMessagePipeHandle handle) {
  if (is_paused_) {
    pending_interface_requests_.emplace(interface_name, std::move(handle));
    return;
  }

  auto iter = name_to_binder_.find(interface_name);
  if (iter != name_to_binder_.end()) {
    iter->second->BindInterface(remote_identity_,
                                interface_name,
                                std::move(handle));
  } else if (!CanBindRequestForInterface(interface_name)) {
    std::stringstream ss;
    ss << "Capability spec prevented service " << remote_identity_.name()
       << " from binding interface: " << interface_name;
    LOG(ERROR) << ss.str();
    mojo::ReportBadMessage(ss.str());
  } else if (!default_binder_.is_null()) {
    default_binder_.Run(interface_name, std::move(handle));
  } else {
    LOG(ERROR) << "Failed to locate a binder for interface: " << interface_name;
  }
}

bool InterfaceRegistry::SetInterfaceBinderForName(
    std::unique_ptr<InterfaceBinder> binder,
    const std::string& interface_name) {
  if (CanBindRequestForInterface(interface_name)) {
    RemoveInterface(interface_name);
    name_to_binder_[interface_name] = std::move(binder);
    return true;
  }
  return false;
}

bool InterfaceRegistry::CanBindRequestForInterface(
    const std::string& interface_name) const {
  return allow_all_interfaces_ ||
      capability_request_.interfaces.count(interface_name);
}

}  // namespace shell
