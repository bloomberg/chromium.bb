// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_INTERFACE_REGISTRY_H_
#define SERVICES_SHELL_PUBLIC_CPP_INTERFACE_REGISTRY_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/lib/interface_factory_binder.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace shell {
class InterfaceBinder;

// An implementation of mojom::InterfaceProvider that allows the user to
// register services to be exposed to another application.
//
// To use, define a class that implements your specific interface. Then
// implement an InterfaceFactory<Foo> that binds instances of FooImpl to
// InterfaceRequest<Foo>s and register that on the registry like this:
//
//   registry.AddInterface(&factory);
//
// Or, if you have multiple factories implemented by the same type, explicitly
// specify the interface to register the factory for:
//
//   registry.AddInterface<Foo>(&my_foo_and_bar_factory_);
//   registry.AddInterface<Bar>(&my_foo_and_bar_factory_);
//
// The InterfaceFactory must outlive the InterfaceRegistry.
//
// Additionally you may specify a default InterfaceBinder to handle requests for
// interfaces unhandled by any registered InterfaceFactory. Just as with
// InterfaceFactory, the default InterfaceBinder supplied must outlive
// InterfaceRegistry.
//
class InterfaceRegistry : public mojom::InterfaceProvider {
 public:
  class TestApi {
   public:
    explicit TestApi(InterfaceRegistry* registry) : registry_(registry) {}
    ~TestApi() {}

    void SetInterfaceBinderForName(InterfaceBinder* binder,
                                   const std::string& interface_name) {
      registry_->SetInterfaceBinderForName(
          base::WrapUnique(binder), interface_name);
    }

    void RemoveInterfaceBinderForName(const std::string& interface_name) {
      registry_->RemoveInterfaceBinderForName(interface_name);
    }

   private:
    InterfaceRegistry* registry_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Construct with a Connection (which may be null), and create an
  // InterfaceProvider pipe, the client end of which may be obtained by calling
  // TakeClientHandle(). If |connection| is non-null, the Mojo Shell's
  // rules filtering which interfaces are allowed to be exposed to clients are
  // imposed on this registry. If null, they are not.
  explicit InterfaceRegistry(Connection* connection);
  // Construct with an InterfaceProviderRequest and a Connection (which may be
  // null, see note above about filtering).
  InterfaceRegistry(mojom::InterfaceProviderRequest request,
                    Connection* connection);
  ~InterfaceRegistry() override;

  // Takes the client end of the InterfaceProvider pipe created in the
  // constructor.
  mojom::InterfaceProviderPtr TakeClientHandle();

  template <typename Interface>
  bool AddInterface(InterfaceFactory<Interface>* factory) {
    return SetInterfaceBinderForName(
        base::WrapUnique(
            new internal::InterfaceFactoryBinder<Interface>(factory)),
        Interface::Name_);
  }

 private:
  using NameToInterfaceBinderMap =
      std::map<std::string, std::unique_ptr<InterfaceBinder>>;

  // mojom::InterfaceProvider:
  void GetInterface(const mojo::String& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override;

  // Returns true if the binder was set, false if it was not set (e.g. by
  // some filtering policy preventing this interface from being exposed).
  bool SetInterfaceBinderForName(std::unique_ptr<InterfaceBinder> binder,
                                 const std::string& name);

  void RemoveInterfaceBinderForName(const std::string& interface_name);

  mojom::InterfaceProviderPtr client_handle_;
  mojo::Binding<mojom::InterfaceProvider> binding_;
  Connection* connection_;

  NameToInterfaceBinderMap name_to_binder_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceRegistry);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_INTERFACE_REGISTRY_H_
