// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_REGISTRY_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_REGISTRY_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/lib/callback_binder.h"
#include "services/service_manager/public/cpp/lib/interface_factory_binder.h"

namespace service_manager {

class Identity;
class InterfaceBinder;

class BinderRegistry {
 public:
  using Binder = base::Callback<void(const std::string&,
                                mojo::ScopedMessagePipeHandle)>;

  BinderRegistry();
  ~BinderRegistry();

  // Provide a factory to be called when a request to bind |Interface| is
  // received by this registry.
  template <typename Interface>
  void AddInterface(InterfaceFactory<Interface>* factory) {
    SetInterfaceBinder(
        Interface::Name_,
        base::MakeUnique<internal::InterfaceFactoryBinder<Interface>>(factory));
  }

  // Provide a callback to be run when a request to bind |Interface| is received
  // by this registry.
  template <typename Interface>
  void AddInterface(
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner =
          nullptr) {
    SetInterfaceBinder(
        Interface::Name_,
        base::MakeUnique<internal::CallbackBinder<Interface>>(callback,
                                                              task_runner));
  }
  void AddInterface(
      const std::string& interface_name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)>& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner = nullptr);

  // Removes the specified interface from the registry. This has no effect on
  // bindings already completed.
  template <typename Interface>
  void RemoveInterface() {
    RemoveInterface(Interface::Name_);
  }
  void RemoveInterface(const std::string& interface_name);

  // Returns true if an InterfaceBinder is registered for |interface_name|.
  bool CanBindInterface(const std::string& interface_name) const;

  // Completes binding the request for |interface_name| on |interface_pipe|, by
  // invoking the corresponding InterfaceBinder.
  void BindInterface(const Identity& remote_identity,
                     const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe);

 private:
  using InterfaceNameToBinderMap =
      std::map<std::string, std::unique_ptr<InterfaceBinder>>;

  // Adds |binder| to the internal map.
  void SetInterfaceBinder(const std::string& interface_name,
                          std::unique_ptr<InterfaceBinder> binder);

  InterfaceNameToBinderMap binders_;

  DISALLOW_COPY_AND_ASSIGN(BinderRegistry);
};


}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_BINDER_REGISTRY_H_
