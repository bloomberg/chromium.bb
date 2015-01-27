// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/service_provider_impl.h"

#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/cpp/environment/logging.h"

namespace mojo {

ServiceProviderImpl::ServiceProviderImpl() : binding_(this) {
}

ServiceProviderImpl::ServiceProviderImpl(
    InterfaceRequest<ServiceProvider> request)
    : binding_(this, request.Pass()) {
}

ServiceProviderImpl::~ServiceProviderImpl() {
}

void ServiceProviderImpl::Bind(InterfaceRequest<ServiceProvider> request) {
  binding_.Bind(request.Pass());
}

void ServiceProviderImpl::ConnectToService(
    const String& service_name,
    ScopedMessagePipeHandle client_handle) {
  if (service_connectors_.find(service_name) == service_connectors_.end()) {
    client_handle.reset();
    return;
  }

  internal::ServiceConnectorBase* service_connector =
      service_connectors_[service_name];
  return service_connector->ConnectToService(service_name,
                                             client_handle.Pass());
}

void ServiceProviderImpl::AddServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  RemoveServiceConnector(service_connector);
  service_connectors_[service_connector->name()] = service_connector;
  // TODO(beng): perhaps take app connection thru ctor??
  service_connector->set_application_connection(nullptr);
}

void ServiceProviderImpl::RemoveServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  NameToServiceConnectorMap::iterator it =
      service_connectors_.find(service_connector->name());
  if (it == service_connectors_.end())
    return;
  delete it->second;
  service_connectors_.erase(it);
}

}  // namespace mojo
