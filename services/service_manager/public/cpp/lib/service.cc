// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service.h"

#include "base/logging.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "services/service_manager/public/interfaces/interface_provider_spec.mojom.h"

namespace service_manager {

Service::Service() = default;

Service::~Service() = default;

void Service::OnStart() {}

bool Service::OnConnect(const ServiceInfo& remote_info,
                        InterfaceRegistry* registry) {
  return false;
}

void Service::OnBindInterface(const ServiceInfo& source_info,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {
  // TODO(beng): Eliminate this implementation once everyone is migrated to
  //             OnBindInterface().
  mojom::InterfaceProviderPtr interface_provider;
  InterfaceProviderSpec source_spec, target_spec;
  GetInterfaceProviderSpec(
      mojom::kServiceManager_ConnectorSpec,
      service_context_->local_info().interface_provider_specs,
      &target_spec);
  GetInterfaceProviderSpec(
      mojom::kServiceManager_ConnectorSpec,
      source_info.interface_provider_specs,
      &source_spec);
  service_context_->CallOnConnect(source_info, source_spec, target_spec,
                                  MakeRequest(&interface_provider));
  interface_provider->GetInterface(interface_name, std::move(interface_pipe));
}

bool Service::OnServiceManagerConnectionLost() {
  return true;
}

ServiceContext* Service::context() const {
  DCHECK(service_context_)
      << "Service::context() may only be called after the Service constructor.";
  return service_context_;
}

void Service::SetContext(ServiceContext* context) {
  service_context_ = context;
}

ForwardingService::ForwardingService(Service* target) : target_(target) {}

ForwardingService::~ForwardingService() {}

void ForwardingService::OnStart() {
  target_->OnStart();
}

bool ForwardingService::OnConnect(const ServiceInfo& remote_info,
                                  InterfaceRegistry* registry) {
  return target_->OnConnect(remote_info, registry);
}

void ForwardingService::OnBindInterface(
    const ServiceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  target_->OnBindInterface(remote_info, interface_name,
                           std::move(interface_pipe));
}

bool ForwardingService::OnServiceManagerConnectionLost() {
  return target_->OnServiceManagerConnectionLost();
}

void ForwardingService::SetContext(ServiceContext* context) {
  target_->SetContext(context);
}

}  // namespace service_manager
