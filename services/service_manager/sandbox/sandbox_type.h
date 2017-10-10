// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_SANDBOX_TYPE_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "services/service_manager/sandbox/export.h"

namespace service_manager {

// Defines the sandbox types known within the servicemanager.
enum SandboxType {
  // Not a valid sandbox type.
  SANDBOX_TYPE_INVALID = -1,

  SANDBOX_TYPE_FIRST_TYPE = 0,  // Placeholder to ease iteration.

  // Do not apply any sandboxing to the process.
  SANDBOX_TYPE_NO_SANDBOX = SANDBOX_TYPE_FIRST_TYPE,

  // Renderer or worker process. Most common case.
  SANDBOX_TYPE_RENDERER,

  // Utility process is as restrictive as the worker process except full
  // access is allowed to one configurable directory.
  SANDBOX_TYPE_UTILITY,

  // GPU process.
  SANDBOX_TYPE_GPU,

  // The PPAPI plugin process.
  SANDBOX_TYPE_PPAPI,

  // The network service process.
  SANDBOX_TYPE_NETWORK,

  // The CDM service process.
  SANDBOX_TYPE_CDM,

#if defined(OS_MACOSX)
  // The NaCl loader process.
  SANDBOX_TYPE_NACL_LOADER,
#endif  // defined(OS_MACOSX)

  // The pdf compositor service process.
  SANDBOX_TYPE_PDF_COMPOSITOR,

  // The profiling service process.
  SANDBOX_TYPE_PROFILING,

  SANDBOX_TYPE_AFTER_LAST_TYPE,  // Placeholder to ease iteration.
};

inline bool IsUnsandboxedSandboxType(SandboxType sandbox_type) {
  // TODO(tsepez): Sandbox network process.
  return sandbox_type == SANDBOX_TYPE_NO_SANDBOX ||
         sandbox_type == SANDBOX_TYPE_NETWORK;
}

SERVICE_MANAGER_SANDBOX_EXPORT void SetCommandLineFlagsForSandboxType(
    base::CommandLine* command_line,
    SandboxType sandbox_type);

SERVICE_MANAGER_SANDBOX_EXPORT SandboxType
SandboxTypeFromCommandLine(const base::CommandLine& command_line);

SERVICE_MANAGER_SANDBOX_EXPORT SandboxType
UtilitySandboxTypeFromString(const std::string& sandbox_string);

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_SANDBOX_TYPE_H_
