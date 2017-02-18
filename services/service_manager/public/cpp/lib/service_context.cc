// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_context.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/lib/connector_impl.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, public:

ServiceContext::ServiceContext(
    std::unique_ptr<service_manager::Service> service,
    mojom::ServiceRequest request,
    std::unique_ptr<Connector> connector,
    mojom::ConnectorRequest connector_request)
    : pending_connector_request_(std::move(connector_request)),
      service_(std::move(service)),
      binding_(this, std::move(request)),
      connector_(std::move(connector)),
      weak_factory_(this) {
  DCHECK(binding_.is_bound());
  binding_.set_connection_error_handler(
      base::Bind(&ServiceContext::OnConnectionError, base::Unretained(this)));
  if (!connector_) {
    connector_ = Connector::Create(&pending_connector_request_);
  } else {
    DCHECK(pending_connector_request_.is_pending());
  }
  service_->SetContext(this);
}

ServiceContext::~ServiceContext() {}

void ServiceContext::SetQuitClosure(const base::Closure& closure) {
  if (service_quit_) {
    // CAUTION: May delete |this|.
    closure.Run();
  } else {
    quit_closure_ = closure;
  }
}

void ServiceContext::RequestQuit() {
  DCHECK(service_control_.is_bound());
  service_control_->RequestQuit();
}

void ServiceContext::DisconnectFromServiceManager() {
  if (binding_.is_bound())
    binding_.Close();
  connector_.reset();
}

void ServiceContext::QuitNow() {
  service_quit_ = true;
  if (binding_.is_bound())
    binding_.Close();
  if (!quit_closure_.is_null()) {
    // CAUTION: May delete |this|.
    base::ResetAndReturn(&quit_closure_).Run();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, mojom::Service implementation:

void ServiceContext::OnStart(const ServiceInfo& info,
                             const OnStartCallback& callback) {
  local_info_ = info;
  callback.Run(std::move(pending_connector_request_),
               mojo::MakeRequest(&service_control_));
  service_->OnStart();
}

void ServiceContext::OnConnect(
    const ServiceInfo& source_info,
    mojom::InterfaceProviderRequest interfaces,
    const OnConnectCallback& callback) {
  InterfaceProviderSpec source_spec, target_spec;
  GetInterfaceProviderSpec(mojom::kServiceManager_ConnectorSpec,
                           local_info_.interface_provider_specs, &target_spec);
  GetInterfaceProviderSpec(mojom::kServiceManager_ConnectorSpec,
                           source_info.interface_provider_specs, &source_spec);

  // Acknowledge the request regardless of whether it's accepted.
  callback.Run();

  CallOnConnect(source_info, source_spec, target_spec, std::move(interfaces));
}

void ServiceContext::OnBindInterface(
    const ServiceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    const OnBindInterfaceCallback& callback) {
  // Acknowledge the request regardless of whether it's accepted.
  callback.Run();

  service_->OnBindInterface(source_info, interface_name,
                            std::move(interface_pipe));
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, private:

void ServiceContext::CallOnConnect(const ServiceInfo& source_info,
                                   const InterfaceProviderSpec& source_spec,
                                   const InterfaceProviderSpec& target_spec,
                                   mojom::InterfaceProviderRequest interfaces) {
  auto registry =
      base::MakeUnique<InterfaceRegistry>(mojom::kServiceManager_ConnectorSpec);
  registry->Bind(std::move(interfaces), local_info_.identity, target_spec,
                 source_info.identity, source_spec);

  if (!service_->OnConnect(source_info, registry.get()))
    return;

  InterfaceRegistry* raw_registry = registry.get();
  registry->AddConnectionLostClosure(base::Bind(
      &ServiceContext::OnRegistryConnectionError, base::Unretained(this),
      raw_registry));
  connection_interface_registries_.insert(
      std::make_pair(raw_registry, std::move(registry)));
}

void ServiceContext::OnConnectionError() {
  if (service_->OnServiceManagerConnectionLost()) {
    // CAUTION: May delete |this|.
    QuitNow();
  }
}

void ServiceContext::OnRegistryConnectionError(InterfaceRegistry* registry) {
  // NOTE: We destroy the InterfaceRegistry asynchronously since it's calling
  // into us from its own connection error handler which may continue to access
  // the InterfaceRegistry's own state after we return.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceContext::DestroyConnectionInterfaceRegistry,
                 weak_factory_.GetWeakPtr(), registry));
}

void ServiceContext::DestroyConnectionInterfaceRegistry(
    InterfaceRegistry* registry) {
  auto it = connection_interface_registries_.find(registry);
  CHECK(it != connection_interface_registries_.end());
  connection_interface_registries_.erase(it);
}

}  // namespace service_manager
