// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_EXECUTABLE_SERVICE_EXECUTABLE_ENVIRONMENT_H_
#define SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_EXECUTABLE_SERVICE_EXECUTABLE_ENVIRONMENT_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace service_manager {

// Sets up boilerplate process environment suitable for most service processes,
// including initialization of //base objects, Mojo IPC, and the scheduler; and
// acceptance of a connection from the Service Manager via canonical
// command-line arguments the Service Manager passes when launching service
// executables. This also starts the sandbox on Linux.
//
// This must outlive any Service implementation that is run within the process.
//
// Note that service executables typically won't use this directly, but will
// instead build a service_executable() target which provides a |ServiceMain()|
// entry point that already runs within the extent of a
// ServiceExecutableEnvironment.
class ServiceExecutableEnvironment {
 public:
  ServiceExecutableEnvironment();
  ~ServiceExecutableEnvironment();

  // Returns a ServiceRequest which should be passed to the Service
  // implementation which will run within the extent of this environment.
  mojom::ServiceRequest TakeServiceRequestFromCommandLine();

 private:
  base::Thread ipc_thread_;
  base::Optional<mojo::core::ScopedIPCSupport> ipc_support_;

  DISALLOW_COPY_AND_ASSIGN(ServiceExecutableEnvironment);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_PUBLIC_CPP_SERVICE_EXECUTABLE_SERVICE_EXECUTABLE_ENVIRONMENT_H_
