// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

namespace service_manager {

class InterfaceRegistry;
class ServiceContext;
struct ServiceInfo;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests. Every Service must minimally
// implement OnConnect().
class Service {
 public:
  virtual ~Service();

  // Called once a bidirectional connection with the Service Manager has been
  // established.
  //
  // |context| is the ServiceContext for this instance of the service. It's
  // guaranteed to outlive the Service instance and therefore may be retained by
  // it.
  //
  // Use the context to retrieve information about the service instance, make
  // outgoing service connections, and issue other requests to the Service
  // Manager on behalf of this instance.
  //
  // Called exactly once before any calls to OnConnect().
  virtual void OnStart(ServiceContext* context);

  // Called each time a connection to this service is brokered by the Service
  // Manager. Implement this to expose interfaces to other services.
  //
  // Return true if the connection should succeed or false if the connection
  // should be rejected.
  virtual bool OnConnect(const ServiceInfo& remote_info,
                         InterfaceRegistry* registry) = 0;

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to shut down, and in fact its process
  // may be reaped shortly afterward if applicable.
  //
  // Return true from this method to tell the ServiceContext to signal its
  // shutdown extenrally (i.e. to invoke it's "connection lost" closure if set),
  // or return false to defer the signal. If deferred, the Service should
  // explicitly call QuitNow() on the ServiceContext when it's ready to be
  // torn down.
  //
  // The default implementation returns true.
  //
  // While it's possible for this to be invoked before either OnStart() or
  // OnConnect() is invoked, neither will be invoked at any point after this
  // OnStop().
  virtual bool OnStop();
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
