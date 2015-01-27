// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_SERVICE_PROVIDER_IMPL_H_
#define MOJO_PUBLIC_APPLICATION_SERVICE_PROVIDER_IMPL_H_

#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {
namespace internal {
class ServiceConnectorBase;
}

// Implements a registry that can be used to expose services to another app.
class ServiceProviderImpl : public ServiceProvider {
 public:
  ServiceProviderImpl();
  explicit ServiceProviderImpl(InterfaceRequest<ServiceProvider> request);
  ~ServiceProviderImpl() override;

  void Bind(InterfaceRequest<ServiceProvider> request);

  template <typename Interface>
  void AddService(InterfaceFactory<Interface>* factory) {
    AddServiceConnector(
        new internal::InterfaceFactoryConnector<Interface>(factory));
  }

 private:
  typedef std::map<std::string, internal::ServiceConnectorBase*>
      NameToServiceConnectorMap;

  // Overridden from ServiceProvider:
  void ConnectToService(const String& service_name,
                        ScopedMessagePipeHandle client_handle) override;

  void AddServiceConnector(internal::ServiceConnectorBase* service_connector);
  void RemoveServiceConnector(
      internal::ServiceConnectorBase* service_connector);

  NameToServiceConnectorMap service_connectors_;

  Binding<ServiceProvider> binding_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceProviderImpl);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_SERVICE_PROVIDER_IMPL_H_
