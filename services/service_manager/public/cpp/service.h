// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace service_manager {

class ServiceContext;
struct BindSourceInfo;

// The primary contract between a Service and the Service Manager, receiving
// lifecycle notifications and connection requests.
class COMPONENT_EXPORT(SERVICE_MANAGER_CPP) Service {
 public:
  Service();
  virtual ~Service();

  // Transfers ownership of |service| to itself such that self-termination via
  // |Terminate()| is also self-deletion. Note that most services implicitly
  // call |Terminate()| when disconnected from the Service Manager, via the
  // default implementation of |OnDisconnected()|.
  //
  // This should really only be called on a Service instance that has a bound
  // connection to the Service Manager, e.g. a functioning ServiceBinding. If
  // the service never calls |Terminate()|, it will effectively leak.
  static void RunUntilTermination(std::unique_ptr<Service> service);

  // Sets a closure to run when the service wants to self-terminate. This may be
  // used by whomever created the Service instance in order to clean up
  // associated resources.
  void set_termination_closure(base::OnceClosure callback) {
    termination_closure_ = std::move(callback);
  }

  // Called exactly once when a bidirectional connection with the Service
  // Manager has been established. No calls to OnBindInterface() will be made
  // before this.
  virtual void OnStart();

  // Called when the service identified by |source.identity| requests this
  // service bind a request for |interface_name|. If this method has been
  // called, the service manager has already determined that policy permits this
  // interface to be bound, so the implementation of this method can trust that
  // it should just blindly bind it under most conditions.
  virtual void OnBindInterface(const BindSourceInfo& source,
                               const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe);

  // Called when the Service Manager has stopped tracking this instance. Once
  // invoked, no further Service interface methods will be called on this
  // Service, and no further communication with the Service Manager is possible.
  //
  // The Service may continue to operate and service existing client connections
  // as it deems appropriate. The default implementation invokes |Terminate()|.
  virtual void OnDisconnected();

  // Called when the Service Manager has stopped tracking this instance. The
  // service should use this as a signal to shut down, and in fact its process
  // may be reaped shortly afterward if applicable.
  //
  // If this returns |true| then QuitNow() will be invoked immediately upon
  // return to the ServiceContext. Otherwise the Service is responsible for
  // eventually calling QuitNow().
  //
  // The default implementation returns |true|.
  //
  // NOTE: This may be called at any time, and once it's been called, none of
  // the other public Service methods will be invoked by the ServiceContext.
  //
  // This is ONLY invoked when using a ServiceContext and is therefore
  // deprecated.
  virtual bool OnServiceManagerConnectionLost();

 protected:
  // Subclasses should always invoke |Terminate()| when they want to
  // self-terminate. This should generally only be done once the service is
  // disconnected from the Service Manager and has no outstanding interface
  // connections servicing clients. Calling |Terminate()| should be considered
  // roughly equivalent to calling |exit(0)| in a normal POSIX process
  // environment, except that services allow for the host environment to define
  // exactly what termination means (see |set_termination_closure| above).
  //
  // Note that if no termination closure is set on this Service instance,
  // calls to |Terminate()| do nothing.
  //
  // As a general rule, subclasses should *ALWAYS* assume that |Terminate()| may
  // delete |*this| before returning.
  void Terminate();

  // Accesses the ServiceContext associated with this Service. Note that this is
  // only valid AFTER the Service's constructor has run.
  ServiceContext* context() const;

 private:
  friend class ServiceContext;
  friend class TestServiceDecorator;

  base::OnceClosure termination_closure_;

  // NOTE: This MUST be called before any public Service methods. ServiceContext
  // satisfies this guarantee for any Service instance it owns.
  virtual void SetContext(ServiceContext* context);

  ServiceContext* service_context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_H_
