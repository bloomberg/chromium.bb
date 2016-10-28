// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/interface_registry.h"

#include <sstream>

#include "mojo/public/cpp/bindings/message.h"
#include "services/service_manager/public/cpp/connection.h"

namespace service_manager {
namespace {

// Returns the set of capabilities required from the target.
CapabilitySet GetRequestedCapabilities(const InterfaceProviderSpec& source_spec,
                                       const Identity& target) {
  CapabilitySet capabilities;

  // Start by looking for specs specific to the supplied identity.
  auto it = source_spec.requires.find(target.name());
  if (it != source_spec.requires.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }

  // Apply wild card rules too.
  it = source_spec.requires.find("*");
  if (it != source_spec.requires.end()) {
    std::copy(it->second.begin(), it->second.end(),
              std::inserter(capabilities, capabilities.begin()));
  }
  return capabilities;
}

// Generates a single set of interfaces that is the union of all interfaces
// exposed by the target for the capabilities requested by the source.
InterfaceSet GetAllowedInterfaces(
    const InterfaceProviderSpec& source_spec,
    const Identity& target,
    const InterfaceProviderSpec& target_spec) {
  InterfaceSet allowed_interfaces;
  // TODO(beng): remove this once we can assert that an InterfaceRegistry must
  //             always be constructed with a valid identity.
  if (!target.IsValid()) {
    allowed_interfaces.insert("*");
    return allowed_interfaces;
  }
  CapabilitySet capabilities = GetRequestedCapabilities(source_spec, target);
  for (const auto& capability : capabilities) {
    auto it = target_spec.provides.find(capability);
    if (it != target_spec.provides.end()) {
      for (const auto& interface_name : it->second)
        allowed_interfaces.insert(interface_name);
    }
  }
  return allowed_interfaces;
}


}  // namespace

InterfaceRegistry::InterfaceRegistry(const std::string& name)
    : binding_(this),
      name_(name),
      weak_factory_(this) {}
InterfaceRegistry::~InterfaceRegistry() {}

void InterfaceRegistry::Bind(
    mojom::InterfaceProviderRequest local_interfaces_request,
    const Identity& local_identity,
    const InterfaceProviderSpec& local_interface_provider_spec,
    const Identity& remote_identity,
    const InterfaceProviderSpec& remote_interface_provider_spec) {
  DCHECK(!binding_.is_bound());
  identity_ = local_identity;
  interface_provider_spec_ = local_interface_provider_spec;
  remote_identity_ = remote_identity;
  allowed_interfaces_ = GetAllowedInterfaces(remote_interface_provider_spec,
                                             identity_,
                                             interface_provider_spec_);
  allow_all_interfaces_ =
      allowed_interfaces_.size() == 1 && allowed_interfaces_.count("*") == 1;
  if (!allow_all_interfaces_) {
    for (auto it = name_to_binder_.begin(); it != name_to_binder_.end();) {
      if (allowed_interfaces_.count(it->first) == 0)
        it = name_to_binder_.erase(it);
      else
        ++it;
    }
  }
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
    ss << "InterfaceProviderSpec \"" << name_ << "\" prevented service: "
       << remote_identity_.name() << " from binding interface: "
       << interface_name << " exposed by: " << identity_.name();
    LOG(ERROR) << ss.str();
    mojo::ReportBadMessage(ss.str());
  } else if (!default_binder_.is_null()) {
    default_binder_.Run(interface_name, std::move(handle));
  } else {
    LOG(ERROR) << "Failed to locate a binder for interface: " << interface_name
               << " requested by: " << remote_identity_.name()
               << " exposed by: " << identity_.name();
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
  // Any interface may be registered before the registry is bound to a pipe. At
  // bind time, the interfaces exposed will be intersected with the requirements
  // of the source.
  if (!binding_.is_bound())
    return true;
  return allow_all_interfaces_ || allowed_interfaces_.count(interface_name);
}

}  // namespace service_manager
