// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace service_manager {

class Connector;
class Identity;
class InterfaceRegistry;
class ServiceContext;
struct ServiceInfo;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class Service {
 public:
  Service();
  virtual ~Service();

  // Called once a bidirectional connection with the Service Manager has been
  // established.
  // |info| contains information about this instance from the Service Manager
  // and the service manifest.
  // Called exactly once before any calls to OnConnect().
  virtual void OnStart(const ServiceInfo& info);

  // Called when a connection to this service is brokered by the Service
  // Manager. Implement to expose interfaces to the remote service. Return true
  // if the connection should succeed. Return false if the connection should
  // be rejected and the underlying pipe closed. The default implementation
  // returns false.
  virtual bool OnConnect(const ServiceInfo& remote_info,
                         InterfaceRegistry* registry);

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to exit, and in fact its process may
  // be reaped shortly afterward.
  // Return true from this method to tell the ServiceContext to run its
  // connection lost closure if it has one, false to prevent it from being run.
  // The default implementation returns true.
  // When used in conjunction with ApplicationRunner, returning true here quits
  // the message loop created by ApplicationRunner, which results in the service
  // quitting.
  // No calls to either OnStart() nor OnConnect() may be received after this is
  // called. It is however possible for this to be called without OnStart() ever
  // having been called.
  virtual bool OnStop();

  Connector* connector();
  ServiceContext* context();
  void set_context(std::unique_ptr<ServiceContext> context);

 private:
  std::unique_ptr<ServiceContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
