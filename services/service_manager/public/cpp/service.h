// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {

class InterfaceRegistry;
class ServiceContext;
struct ServiceInfo;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class Service {
 public:
  Service();
  virtual ~Service();

  // Called exactly once when a bidirectional connection with the Service
  // Manager has been established. No calls to OnConnect(), OnBindInterface(),
  // or OnStop() will be made before this. Note that this call is mutually
  // exclusive to OnStartFailed() - either one or the other will be the first
  // call on any given Service instance.
  virtual void OnStart();

  // Called if the Service loses its connection to the Service Manager before
  // OnStart() could be invoked. Once this called, none of the other public
  // Service interface methods will be called.  Note that this call is mutually
  // exclusive to OnStart() - either one or the other will be the first call on
  // any given Service instance.
  //
  // The default implementation calls QuitNow() on context().
  virtual void OnStartFailed();

  // Called each time a connection to this service is brokered by the Service
  // Manager. Implement this to expose interfaces to other services.
  //
  // Return true if the connection should succeed or false if the connection
  // should be rejected.
  //
  // The default implementation returns false.
  virtual bool OnConnect(const ServiceInfo& remote_info,
                         InterfaceRegistry* registry);

  // Called when the service identified by |source_info| requests this service
  // bind a request for |interface_name|. If this method has been called, the
  // service manager has already determined that policy permits this interface
  // to be bound, so the implementation of this method can trust that it should
  // just blindly bind it under most conditions.
  virtual void OnBindInterface(const ServiceInfo& source_info,
                               const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe);

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to shut down, and in fact its process
  // may be reaped shortly afterward if applicable.
  //
  // Returning true from this method signals that the Service instance can be
  // destroyed immediately. More precisely, it will cause the context()'s
  // QuitNow() method to be invoked immediately after this OnStop() call.
  //
  // If shutdown is deferred by returning false, the Service itself is
  // responsible for explicitly calling QuitNow() on context() when it's ready
  // to be destroyed.
  //
  // The default implementation returns true.
  //
  // NOTE: This will only be called after OnStart(), and none of the other
  // public Service methods will be called after this.
  virtual bool OnStop();

 protected:
  // Accesses the ServiceContext associated with this Service. Note that this is
  // only valid during or after OnStart() or OnStartFailed(), but never before.
  ServiceContext* context() const;

 private:
  friend class ForwardingService;
  friend class ServiceContext;

  // NOTE: This is guaranteed to be called before OnStart().
  void set_context(ServiceContext* context) { service_context_ = context; }

  ServiceContext* service_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

// TODO(rockot): Remove this. It's here to satisfy a few remaining use cases
// where a Service impl is owned by something other than its ServiceContext.
class ForwardingService : public Service {
 public:
  // |target| must outlive this object.
  explicit ForwardingService(Service* target);
  ~ForwardingService() override;

  // Service:
  void OnStart() override;
  bool OnConnect(const ServiceInfo& remote_info,
                 InterfaceRegistry* registry) override;
  void OnBindInterface(const ServiceInfo& remote_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;
  bool OnStop() override;
  void OnStartFailed() override;

 private:
  Service* const target_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ForwardingService);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
