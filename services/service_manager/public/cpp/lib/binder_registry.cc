// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/binder_registry.h"

#include "services/service_manager/public/cpp/identity.h"

namespace service_manager {

BinderRegistry::BinderRegistry() {}
BinderRegistry::~BinderRegistry() {}

void BinderRegistry::AddInterface(
    const std::string& interface_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  SetInterfaceBinder(
      interface_name,
      base::MakeUnique<internal::GenericCallbackBinder>(callback, task_runner));
}

void BinderRegistry::RemoveInterface(const std::string& interface_name) {
  auto it = binders_.find(interface_name);
  if (it != binders_.end())
    binders_.erase(it);
}

bool BinderRegistry::CanBindInterface(const std::string& interface_name) const {
  auto it = binders_.find(interface_name);
  return it != binders_.end();
}

void BinderRegistry::BindInterface(
    const Identity& remote_identity,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  auto it = binders_.find(interface_name);
  if (it != binders_.end()) {
    it->second->BindInterface(remote_identity, interface_name,
                              std::move(interface_pipe));
  } else {
    LOG(ERROR) << "Failed to locate a binder for interface: " << interface_name;
  }
}

void BinderRegistry::SetInterfaceBinder(
    const std::string& interface_name,
    std::unique_ptr<InterfaceBinder> binder) {
  RemoveInterface(interface_name);
  binders_[interface_name] = std::move(binder);
}

}  // namespace service_manager
