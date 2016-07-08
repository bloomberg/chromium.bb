// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_SERVICE_CONTEXT_H_
#define SERVICES_SHELL_PUBLIC_CPP_SERVICE_CONTEXT_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace shell {

class Connector;

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
//   the Service Manager, typically in response to either MojoMain() or main().
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
  // take ownership of |client|, which must remain valid for the lifetime of
  // ServiceContext.
  ServiceContext(shell::Service* client,
                 mojom::ServiceRequest request);

  ~ServiceContext() override;

  Connector* connector() { return connector_.get(); }
  const Identity& identity() { return identity_; }

  // TODO(rockot): Remove this. http://crbug.com/594852.
  void set_initialize_handler(const base::Closure& callback);

  // TODO(rockot): Remove this once we get rid of app tests.
  void SetAppTestConnectorForTesting(mojom::ConnectorPtr connector);

  // Specify a function to be called when the connection to the shell is lost.
  // Note that if connection has already been lost, then |closure| is called
  // immediately.
  void SetConnectionLostClosure(const base::Closure& closure);

 private:
  // mojom::Service:
  void OnStart(mojom::IdentityPtr identity,
               uint32_t id,
               const OnStartCallback& callback) override;
  void OnConnect(mojom::IdentityPtr source,
                 uint32_t source_id,
                 mojom::InterfaceProviderRequest remote_interfaces,
                 mojom::InterfaceProviderPtr local_interfaces,
                 mojom::CapabilityRequestPtr allowed_capabilities,
                 const mojo::String& name) override;

  void OnConnectionError();

  // A callback called when OnStart() is run.
  base::Closure initialize_handler_;

  // We track the lifetime of incoming connection registries as it more
  // convenient for the client.
  ScopedVector<Connection> incoming_connections_;

  // A pending Connector request which will eventually be passed to the Service
  // Manager.
  mojom::ConnectorRequest pending_connector_request_;

  shell::Service* client_;
  mojo::Binding<mojom::Service> binding_;
  std::unique_ptr<Connector> connector_;
  shell::Identity identity_;
  bool should_run_connection_lost_closure_ = false;

  base::Closure connection_lost_closure_;

  DISALLOW_COPY_AND_ASSIGN(ServiceContext);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_SERVICE_CONTEXT_H_
