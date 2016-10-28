// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/service_context.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/lib/connector_impl.h"
#include "services/service_manager/public/cpp/service.h"

namespace service_manager {

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, public:

ServiceContext::ServiceContext(service_manager::Service* service,
                               mojom::ServiceRequest request,
                               std::unique_ptr<Connector> connector,
                               mojom::ConnectorRequest connector_request)
    : pending_connector_request_(std::move(connector_request)),
      service_(service),
      binding_(this, std::move(request)),
      connector_(std::move(connector)) {
  DCHECK(binding_.is_bound());
  binding_.set_connection_error_handler(
      base::Bind(&ServiceContext::OnConnectionError, base::Unretained(this)));
  if (!connector_) {
    connector_ = Connector::Create(&pending_connector_request_);
  } else {
    DCHECK(pending_connector_request_.is_pending());
  }
}

ServiceContext::~ServiceContext() {}

void ServiceContext::SetConnectionLostClosure(const base::Closure& closure) {
  connection_lost_closure_ = closure;
  if (should_run_connection_lost_closure_ &&
      !connection_lost_closure_.is_null())
    connection_lost_closure_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, mojom::Service implementation:

void ServiceContext::OnStart(const ServiceInfo& info,
                             const OnStartCallback& callback) {
  local_info_ = info;
  if (!initialize_handler_.is_null())
    initialize_handler_.Run();

  callback.Run(std::move(pending_connector_request_));

  service_->OnStart(info);
}

void ServiceContext::OnConnect(
    const ServiceInfo& source_info,
    mojom::InterfaceProviderRequest interfaces) {
  InterfaceProviderSpec source_spec, target_spec;
  GetInterfaceProviderSpec(mojom::kServiceManager_ConnectorSpec,
                           local_info_.interface_provider_specs, &target_spec);
  GetInterfaceProviderSpec(mojom::kServiceManager_ConnectorSpec,
                           source_info.interface_provider_specs, &source_spec);
  auto registry =
      base::MakeUnique<InterfaceRegistry>(mojom::kServiceManager_ConnectorSpec);
  registry->Bind(std::move(interfaces), local_info_.identity, target_spec,
                 source_info.identity, source_spec);

  if (!service_->OnConnect(source_info, registry.get()))
    return;

  // TODO(beng): it appears we never prune this list. We should, when the
  //             registry's remote interface provider pipe breaks.
  incoming_connections_.push_back(std::move(registry));
}

////////////////////////////////////////////////////////////////////////////////
// ServiceContext, private:

void ServiceContext::OnConnectionError() {
  // Note that the Service doesn't technically have to quit now, it may live
  // on to service existing connections. All existing Connectors however are
  // invalid.
  should_run_connection_lost_closure_ = service_->OnStop();
  if (should_run_connection_lost_closure_ &&
      !connection_lost_closure_.is_null())
    connection_lost_closure_.Run();
  // We don't reset the connector as clients may have taken a raw pointer to it.
  // Connect() will return nullptr if they try to connect to anything.
}

}  // namespace service_manager
