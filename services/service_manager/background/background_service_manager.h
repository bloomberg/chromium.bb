// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_BACKGROUND_BACKGROUND_SERVICE_MANAGER_H_
#define SERVICES_SERVICE_MANAGER_BACKGROUND_BACKGROUND_SERVICE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/runner/host/service_process_launcher_delegate.h"

namespace base {
class WaitableEvent;
}

namespace service_manager {

class Context;
class Identity;
class ServiceManager;

// BackgroundServiceManager runs a Service Manager on a dedicated background
// thread.
class BackgroundServiceManager {
 public:
  BackgroundServiceManager(ServiceProcessLauncherDelegate* launcher_delegate,
                           const std::vector<Manifest>& manifests);
  ~BackgroundServiceManager();

  // Creates a service instance for |identity|. This is intended for use by the
  // Service Manager's embedder to register instances directly, without
  // requiring a Connector.
  //
  // |pid_receiver_request| may be null, in which case the service manager
  // assumes the new service is running in this process.
  void RegisterService(const Identity& identity,
                       mojom::ServicePtr service,
                       mojom::PIDReceiverRequest pid_receiver_request);

 private:
  void InitializeOnBackgroundThread(
      ServiceProcessLauncherDelegate* launcher_delegate,
      const std::vector<Manifest>& manifests);
  void ShutDownOnBackgroundThread(base::WaitableEvent* done_event);
  void RegisterServiceOnBackgroundThread(
      const Identity& identity,
      mojom::ServicePtrInfo service_info,
      mojom::PIDReceiverRequest pid_receiver_request);

  base::Thread background_thread_;

  // The ServiceManager context. Must only be used on the background thread.
  std::unique_ptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundServiceManager);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_BACKGROUND_BACKGROUND_SERVICE_MANAGER_H_
