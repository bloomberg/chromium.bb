// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SHELL_PUBLIC_CPP_SERVICE_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/identity.h"

namespace shell {

class Connector;
class ServiceContext;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class Service {
 public:
  Service();
  virtual ~Service();

  // Called once a bidirectional connection with the Service Manager has been
  // established.
  // |identity| is the identity of the service instance.
  // Called exactly once before any other method.
  virtual void OnStart(const Identity& identity);

  // Called when a connection to this service is brokered by the Service
  // Manager. Override to expose interfaces to the remote service. Return true
  // if the connection should succeed. Return false if the connection should
  // be rejected and the underlying pipe closed. The default implementation
  // returns false.
  virtual bool OnConnect(Connection* connection);

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to exit, and in fact its process may
  // be reaped shortly afterward.
  // Return true from this method to tell the ServiceContext to run its
  // connection lost closure if it has one, false to prevent it from being run.
  // The default implementation returns true.
  // When used in conjunction with ApplicationRunner, returning true here quits
  // the message loop created by ApplicationRunner, which results in the service
  // quitting.
  virtual bool OnStop();

  // TODO(rockot): remove
  virtual InterfaceProvider* GetInterfaceProviderForConnection();
  virtual InterfaceRegistry* GetInterfaceRegistryForConnection();

  Connector* connector();
  ServiceContext* context();
  void set_context(std::unique_ptr<ServiceContext> context);

 private:
  std::unique_ptr<ServiceContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_SERVICE_H_
