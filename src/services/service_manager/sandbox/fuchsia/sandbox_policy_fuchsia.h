// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_FUCHSIA_SANDBOX_POLICY_FUCHSIA_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_FUCHSIA_SANDBOX_POLICY_FUCHSIA_H_

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/zx/job.h>

#include "base/memory/ref_counted.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace base {
struct LaunchOptions;
class SequencedTaskRunner;

namespace fuchsia {
class FilteredServiceDirectory;
}  // namespace fuchsia

}  // namespace base

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT SandboxPolicyFuchsia {
 public:
  // Must be called on the IO thread.
  explicit SandboxPolicyFuchsia(service_manager::SandboxType type);
  ~SandboxPolicyFuchsia();

  // Sets the service directory to pass to the child process when launching it.
  // This is only supported for SandboxType::kWebContext processes.  If this is
  // not called for a WEB_CONTEXT process then it will receive no services.
  void SetServiceDirectory(
      fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory_client);

  // Modifies the process launch |options| to achieve  the level of
  // isolation appropriate for current the sandbox type. The caller may then add
  // any descriptors or handles afterward to grant additional capabilities
  // to the new process.
  void UpdateLaunchOptionsForSandbox(base::LaunchOptions* options);

 private:
  service_manager::SandboxType type_;

  // Services directory used for the /svc namespace of the child process.
  std::unique_ptr<base::fuchsia::FilteredServiceDirectory> service_directory_;
  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory_client_;
  scoped_refptr<base::SequencedTaskRunner> service_directory_task_runner_;

  // Job in which the child process is launched.
  zx::job job_;

  DISALLOW_COPY_AND_ASSIGN(SandboxPolicyFuchsia);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_FUCHSIA_SANDBOX_POLICY_FUCHSIA_H_
