// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_PUBLIC_CPP_SERVICE_RUNNER_H_
#define SERVICES_SHELL_PUBLIC_CPP_SERVICE_RUNNER_H_

#include <memory>

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"

namespace shell {

class Service;
class ServiceContext;

// A utility for running a Service that uses //base. The typical use case is to
// use from your ServiceMain:
//
//  MojoResult ServiceMain(MojoHandle service_request_handle) {
//    shell::ServiceRunner runner(new MyService);
//    return runner.Run(service_request_handle);
//  }
//
// ServiceRunner takes care of chromium environment initialization and
// shutdown, and starting a base::MessageLoop from which your service can run
// and ultimately Quit().
class ServiceRunner {
 public:
  // Takes ownership of |service|.
  explicit ServiceRunner(Service* service);
  ~ServiceRunner();

  static void InitBaseCommandLine();

  void set_message_loop_type(base::MessageLoop::Type type);

  // Once the various parameters have been set above, use Run to initialize an
  // ServiceContext wired to the provided delegate, and run a MessageLoop until
  // the service exits.
  //
  // Iff |init_base| is true, the runner will perform some initialization of
  // base globals (e.g. CommandLine and AtExitManager) before starting the
  // service.
  MojoResult Run(MojoHandle shell_handle, bool init_base);

  // Calls Run above with |init_base| set to |true|.
  MojoResult Run(MojoHandle shell_handle);

  // Allows the caller to shut down the connection with the Service Manager.
  // After the Service Manager notices the pipe has closed, it will no longer
  // track an instance of this service, though this service may continue to run
  // and handle requests from others.
  void DestroyServiceContext();

  // Allows the caller to explicitly quit the service. Must be called from
  // the thread which created the ServiceRunner.
  void Quit();

 private:
  std::unique_ptr<Service> service_;

  // MessageLoop type. TYPE_CUSTOM is default (MessagePumpMojo will be used as
  // the underlying message pump).
  base::MessageLoop::Type message_loop_type_;
  // Whether Run() has been called.
  bool has_run_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRunner);
};

}  // namespace shell

#endif  // SERVICES_SHELL_PUBLIC_CPP_SERVICE_RUNNER_H_
