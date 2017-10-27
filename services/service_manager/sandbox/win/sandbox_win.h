// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANANGER_SANDBOX_WIN_SANDBOX_WIN_H_
#define SERVICES_SERVICE_MANANGER_SANDBOX_WIN_SANDBOX_WIN_H_

#include <stdint.h>

#include <string>

#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "sandbox/win/src/sandbox_types.h"
#include "sandbox/win/src/security_level.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/sandbox_delegate.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace base {
class CommandLine;
}  // namespace base

namespace sandbox {
class BrokerServices;
class TargetPolicy;
class TargetServices;
}  // namespace sandbox

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT SandboxWin {
 public:
  static sandbox::ResultCode StartSandboxedProcess(
      base::CommandLine* cmd_line,
      const std::string& process_type,
      const base::HandlesToInheritVector& handles_to_inherit,
      SandboxDelegate* delegate,
      base::Process* process);

  // Wrapper around sandbox::TargetPolicy::SetJobLevel that checks if the
  // sandbox should be let to run without a job object assigned.
  static sandbox::ResultCode SetJobLevel(const base::CommandLine& cmd_line,
                                         sandbox::JobLevel job_level,
                                         uint32_t ui_exceptions,
                                         sandbox::TargetPolicy* policy);

  // Closes handles that are opened at process creation and initialization.
  static sandbox::ResultCode AddBaseHandleClosePolicy(
      sandbox::TargetPolicy* policy);

  // Add AppContainer policy for |sid| on supported OS.
  static sandbox::ResultCode AddAppContainerPolicy(
      sandbox::TargetPolicy* policy,
      const wchar_t* sid);

  // Add the win32k lockdown policy on supported OS.
  static sandbox::ResultCode AddWin32kLockdownPolicy(
      sandbox::TargetPolicy* policy,
      bool enable_opm);

  static bool InitBrokerServices(sandbox::BrokerServices* broker_services);
  static bool InitTargetServices(sandbox::TargetServices* target_services);
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANANGER_SANDBOX_WIN_SANDBOX_WIN_H_
