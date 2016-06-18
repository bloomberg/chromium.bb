// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_REMOTE_INTERFACE_REGISTRY_H_
#define SERVICES_SHELL_PUBLIC_CPP_REMOTE_INTERFACE_REGISTRY_H_

#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {

// Encapsulates a mojom::InterfaceProviderPtr implemented in a remote
// application. Provides two main features:
// - a typesafe GetInterface() method for binding InterfacePtrs.
// - a testing API that allows local callbacks to be registered that bind
//   requests for remote interfaces.
// An instance of this class is used by the GetInterface() methods on
// Connection.
class RemoteInterfaceRegistry {
 public:
  class TestApi {
   public:
    explicit TestApi(RemoteInterfaceRegistry* registry) : registry_(registry) {}
    ~TestApi() {}

    template <typename Interface>
    void SetBinderForName(
        const std::string& name,
        const base::Callback<void(mojo::InterfaceRequest<Interface>)>& binder) {
      registry_->SetBinderForName(name, binder);
    }

    void ClearBinders() {
      registry_->ClearBinders();
    }

   private:
    RemoteInterfaceRegistry* registry_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit RemoteInterfaceRegistry(
      mojom::InterfaceProviderPtr remote_interfaces);
  ~RemoteInterfaceRegistry();

  // Returns a raw pointer to the remote InterfaceProvider.
  mojom::InterfaceProvider* GetInterfaceProvider();

  // Sets a closure to be run when the remote InterfaceProvider pipe is closed.
  void SetConnectionLostClosure(const base::Closure& connection_lost_closure);

  // Binds |ptr| to an implementation of Interface in the remote application.
  // |ptr| can immediately be used to start sending requests to the remote
  // interface.
  template <typename Interface>
  void GetInterface(mojo::InterfacePtr<Interface>* ptr) {
    mojo::MessagePipe pipe;
    ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));

    // Local binders can be registered via TestApi.
    auto it = binders_.find(Interface::Name_);
    if (it != binders_.end()) {
      it->second.Run(std::move(pipe.handle1));
      return;
    }
    remote_interfaces_->GetInterface(Interface::Name_, std::move(pipe.handle1));
  }
  template <typename Interface>
  void GetInterface(mojo::InterfaceRequest<Interface> request) {
    GetInterface(Interface::Name_, std::move(request.PassMessagePipe()));
  }
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle);

 private:
  template <typename Interface>
  void SetBinderForName(
      const std::string& name,
      const base::Callback<void(mojo::InterfaceRequest<Interface>)>& binder) {
    binders_[name] = binder;
  }
  void ClearBinders();

  using BinderMap = std::map<
      std::string, base::Callback<void(mojo::ScopedMessagePipeHandle)>>;
  BinderMap binders_;

  mojom::InterfaceProviderPtr remote_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(RemoteInterfaceRegistry);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_REMOTE_INTERFACE_REGISTRY_H_
