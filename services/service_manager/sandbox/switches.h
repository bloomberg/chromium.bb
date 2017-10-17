// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_SANDBOX_SWITCHES_H_
#define SERVICES_SERVICE_MANAGER_SANDBOX_SWITCHES_H_

#include "build/build_config.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/export.h"

namespace service_manager {
namespace switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kServiceSandboxType[];

// Must be in sync with "sandbox_type" values as used in service manager's
// manifest.json catalog files.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNoneSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNetworkSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPpapiSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kUtilitySandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kCdmSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPdfCompositorSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kProfilingSandbox[];

// Flags owned by the service manager sandbox.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kAllowNoSandboxJob[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kEnableAppContainer[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableAppContainer[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableWin32kLockDown[];

// Flags spied upon from other layers.
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kGpuProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPpapiBrokerProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kPpapiPluginProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kRendererProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kUtilityProcess[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kDisableGpuSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kNoSandbox[];
SERVICE_MANAGER_SANDBOX_EXPORT extern const char kEnableSandboxLogging[];

}  // namespace switches

#if defined(OS_WIN)
// Returns whether Win32k lockdown is enabled for child processes or not.
// Not really a switch, but uses one under the covers.
SERVICE_MANAGER_SANDBOX_EXPORT bool IsWin32kLockdownEnabled();
#endif

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_SANDBOX_SWITCHES_H_
