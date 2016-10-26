// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace service_manager {

// Encapsulates a connection to the Service Manager in two parts:
// - a bound InterfacePtr to mojom::Connector, the primary mechanism
//   by which the instantiating service connects to other services,
//   brokered by the Service Manager.
// - a bound InterfaceRequest of mojom::Service, an interface used by the
//   Service Manager to inform this service of lifecycle events and
//   inbound connections brokered by it.
//
// This class should be used in two scenarios:
// - During early startup to bind the mojom::ServiceRequest obtained from
//   the Service Manager, typically in response to either ServiceMain() or
//   main().
// - In an implementation of mojom::ServiceFactory to bind the
//   mojom::ServiceRequest passed via CreateService. In this scenario there can
//   be many instances of this class per process.
//
// Instances of this class are constructed with an implementation of the Service
// Manager Client Lib's Service interface. See documentation in service.h
// for details.
//
class ServiceContext : public mojom::Service {
 public:
  // Creates a new ServiceContext bound to |request|. This connection may be
  // used immediately to make outgoing connections via connector().  Does not
  // take ownership of |service|, which must remain valid for the lifetime of
  // ServiceContext. If either |connector| or |connector_request| is non-null
  // both must be non-null. If both are null, the connection will create its own
  // Connector and request to pass to the Service Manager on initialization.
  ServiceContext(service_manager::Service* service,
                 mojom::ServiceRequest request,
                 std::unique_ptr<Connector> connector = nullptr,
                 mojom::ConnectorRequest connector_request = nullptr);

  ~ServiceContext() override;

  Connector* connector() { return connector_.get(); }
  const Identity& identity() { return local_info_.identity; }

  // Specify a function to be called when the connection to the service manager
  // is lost.
  // Note that if connection has already been lost, then |closure| is called
  // immediately.
  void SetConnectionLostClosure(const base::Closure& closure);

 private:
  // mojom::Service:
  void OnStart(const ServiceInfo& info,
               const OnStartCallback& callback) override;
  void OnConnect(const ServiceInfo& source_info,
                 mojom::InterfaceProviderRequest interfaces) override;

  void OnConnectionError();

  // A callback called when OnStart() is run.
  base::Closure initialize_handler_;

  // We track the lifetime of incoming connection registries as it more
  // convenient for the client.
  std::vector<std::unique_ptr<InterfaceRegistry>> incoming_connections_;

  // A pending Connector request which will eventually be passed to the Service
  // Manager.
  mojom::ConnectorRequest pending_connector_request_;

  service_manager::Service* service_;
  mojo::Binding<mojom::Service> binding_;
  std::unique_ptr<Connector> connector_;
  service_manager::ServiceInfo local_info_;
  bool should_run_connection_lost_closure_ = false;

  base::Closure connection_lost_closure_;

  DISALLOW_COPY_AND_ASSIGN(ServiceContext);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_CONTEXT_H_
