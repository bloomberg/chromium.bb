// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/lib/service_registry.h"

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/lib/service_connector.h"

namespace mojo {
namespace internal {

ServiceRegistry::ServiceRegistry(
    ApplicationImpl* application_impl,
    const std::string& url,
    ServiceProviderPtr remote_services,
    InterfaceRequest<ServiceProvider> local_services)
    : application_impl_(application_impl),
      url_(url),
      local_binding_(this),
      remote_service_provider_(remote_services.Pass()) {
  if (local_services.is_pending())
    local_binding_.Bind(local_services.Pass());
}

ServiceRegistry::ServiceRegistry()
    : application_impl_(nullptr), local_binding_(this) {
}

ServiceRegistry::~ServiceRegistry() {
  for (NameToServiceConnectorMap::iterator i =
           name_to_service_connector_.begin();
       i != name_to_service_connector_.end();
       ++i) {
    delete i->second;
  }
  name_to_service_connector_.clear();
}

void ServiceRegistry::AddServiceConnector(
    ServiceConnectorBase* service_connector) {
  RemoveServiceConnectorInternal(service_connector);
  name_to_service_connector_[service_connector->name()] = service_connector;
  service_connector->set_application_connection(this);
}

void ServiceRegistry::RemoveServiceConnector(
    ServiceConnectorBase* service_connector) {
  RemoveServiceConnectorInternal(service_connector);
  if (name_to_service_connector_.empty())
    remote_service_provider_.reset();
}

bool ServiceRegistry::RemoveServiceConnectorInternal(
    ServiceConnectorBase* service_connector) {
  NameToServiceConnectorMap::iterator it =
      name_to_service_connector_.find(service_connector->name());
  if (it == name_to_service_connector_.end())
    return false;
  delete it->second;
  name_to_service_connector_.erase(it);
  return true;
}

const std::string& ServiceRegistry::GetRemoteApplicationURL() {
  return url_;
}

ServiceProvider* ServiceRegistry::GetServiceProvider() {
  return remote_service_provider_.get();
}

void ServiceRegistry::ConnectToService(const mojo::String& service_name,
                                       ScopedMessagePipeHandle client_handle) {
  if (name_to_service_connector_.find(service_name) ==
      name_to_service_connector_.end()) {
    client_handle.reset();
    return;
  }
  internal::ServiceConnectorBase* service_connector =
      name_to_service_connector_[service_name];
  return service_connector->ConnectToService(service_name,
                                             client_handle.Pass());
}

}  // namespace internal
}  // namespace mojo
