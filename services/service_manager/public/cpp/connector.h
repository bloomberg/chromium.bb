// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_

#include <memory>

#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "services/service_manager/public/interfaces/service_manager.mojom.h"

namespace service_manager {

// An interface that encapsulates the Service Manager's brokering interface, by
// which
// connections between services are established. Once Connect() is called,
// this class is bound to the thread the call was made on and it cannot be
// passed to another thread without calling Clone().
//
// An instance of this class is created internally by ServiceContext for use
// on the thread ServiceContext is instantiated on.
//
// To use this interface on another thread, call Clone() and pass the new
// instance to the desired thread before calling Connect().
//
// While instances of this object are owned by the caller, the underlying
// connection with the service manager is bound to the lifetime of the instance
// that
// created it, i.e. when the application is terminated the Connector pipe is
// closed.
class Connector {
 public:
  virtual ~Connector() {}

  // Creates a new Connector instance and fills in |*request| with a request
  // for the other end the Connector's interface.
  static std::unique_ptr<Connector> Create(mojom::ConnectorRequest* request);

  // Creates an instance of a service for |identity| in a process started by the
  // client (or someone else). Must be called before Connect() may be called to
  // |identity|.
  virtual void StartService(
      const Identity& identity,
      mojom::ServicePtr service,
      mojom::PIDReceiverRequest pid_receiver_request) = 0;

  // Requests a new connection to a service. Returns a pointer to the
  // connection if the connection is permitted by that service, nullptr
  // otherwise. Once this method is called, this object is bound to the thread
  // on which the call took place. To pass to another thread, call Clone() and
  // pass the result.
  virtual std::unique_ptr<Connection> Connect(const std::string& name) = 0;
  virtual std::unique_ptr<Connection> Connect(const Identity& target) = 0;

  // Connect to |target| & request to bind |Interface|.
  template <typename Interface>
  void BindInterface(const Identity& target,
                     mojo::InterfacePtr<Interface>* ptr) {
    mojo::MessagePipe pipe;
    ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));
    BindInterface(target, Interface::Name_, std::move(pipe.handle1));
  }
  template <typename Interface>
  void BindInterface(const std::string& name,
                     mojo::InterfacePtr<Interface>* ptr) {
    return BindInterface(Identity(name, mojom::kInheritUserID), ptr);
  }
  template <typename Interface>
  void BindInterface(const std::string& name,
                     mojo::InterfaceRequest<Interface> request) {
    return BindInterface(Identity(name, mojom::kInheritUserID),
                         Interface::Name_, request.PassMessagePipe());
  }
  virtual void BindInterface(const Identity& target,
                             const std::string& interface_name,
                             mojo::ScopedMessagePipeHandle interface_pipe) = 0;

  // Creates a new instance of this class which may be passed to another thread.
  // The returned object may be passed multiple times until Connect() is called,
  // at which point this method must be called again to pass again.
  virtual std::unique_ptr<Connector> Clone() = 0;

  // Binds a Connector request to the other end of this Connector.
  virtual void BindConnectorRequest(mojom::ConnectorRequest request) = 0;
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_CONNECTOR_H_
