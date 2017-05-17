// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_STANDALONE_CONTEXT_H_
#define SERVICES_SERVICE_MANAGER_STANDALONE_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "services/service_manager/runner/host/service_process_launcher.h"

namespace base {
class SequencedWorkerPool;
class Value;
}

namespace service_manager {

class ServiceManager;

constexpr size_t kThreadPoolMaxThreads = 3;

// The "global" context for the service manager's main process.
class Context {
 public:
  Context(ServiceProcessLauncher::Delegate* launcher_delegate,
          std::unique_ptr<base::Value> catalog_content);
  ~Context();

  // Run the application specified on the command line.
  void RunCommandLineApplication();

  ServiceManager* service_manager() { return service_manager_.get(); }

 private:
  // Runs the app specified by |name|.
  void Run(const std::string& name);

  scoped_refptr<base::SequencedWorkerPool> blocking_pool_;

  std::unique_ptr<ServiceManager> service_manager_;
  base::Time main_entry_time_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_STANDALONE_CONTEXT_H_
