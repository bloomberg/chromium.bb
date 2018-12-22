// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_STANDALONE_CONTEXT_H_
#define SERVICES_SERVICE_MANAGER_STANDALONE_CONTEXT_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/runner/host/service_process_launcher_delegate.h"

namespace service_manager {

class ServiceManager;

// The "global" context for the service manager's main process.
class Context {
 public:
  Context(ServiceProcessLauncherDelegate* launcher_delegate,
          const std::vector<Manifest>& manifests);
  ~Context();

  // Run the application specified on the command line, and run |on_quit| when
  // the application instance quits.
  void RunCommandLineApplication(base::RepeatingClosure on_quit);

  ServiceManager* service_manager() { return service_manager_.get(); }

 private:
  // Runs the app specified by |name|.
  void Run(const std::string& name, base::RepeatingClosure on_quit);

  std::unique_ptr<ServiceManager> service_manager_;
  base::Time main_entry_time_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_STANDALONE_CONTEXT_H_
