// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/switches.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "base/win/windows_version.h"
#endif

namespace service_manager {
namespace switches {

// Type of sandbox to apply to the process running the service, one of the
// values in the next block.
const char kServiceSandboxType[] = "service-sandbox-type";

// Must be in sync with "sandbox_type" values as used in service manager's
// manifest.json catalog files.
const char kNoneSandbox[] = "none";
const char kNoneSandboxAndElevatedPrivileges[] = "none_and_elevated";
const char kNetworkSandbox[] = "network";
const char kPpapiSandbox[] = "ppapi";
const char kUtilitySandbox[] = "utility";
const char kCdmSandbox[] = "cdm";
const char kPdfCompositorSandbox[] = "pdf_compositor";
const char kProfilingSandbox[] = "profiling";

// Flags owned by the service manager sandbox.

// Enables the sandboxed processes to run without a job object assigned to them.
// This flag is required to allow Chrome to run in RemoteApps or Citrix. This
// flag can reduce the security of the sandboxed processes and allow them to do
// certain API calls like shut down Windows or access the clipboard. Also we
// lose the chance to kill some processes until the outer job that owns them
// finishes.
const char kAllowNoSandboxJob[] = "allow-no-sandbox-job";

// Allows debugging of sandboxed processes (see zygote_main_linux.cc).
const char kAllowSandboxDebugging[] = "allow-sandbox-debugging";

// Disable appcontainer/lowbox for renderer on Win8+ platforms.
const char kDisableAppContainer[] = "disable-appcontainer";

// Disable the seccomp filter sandbox (seccomp-bpf) (Linux only).
const char kDisableSeccompFilterSandbox[] = "disable-seccomp-filter-sandbox";

// Disable the setuid sandbox (Linux only).
const char kDisableSetuidSandbox[] = "disable-setuid-sandbox";

// Disables the Win32K process mitigation policy for child processes.
const char kDisableWin32kLockDown[] = "disable-win32k-lockdown";

// Ensable appcontainer/lowbox for renderer on Win8+ platforms.
const char kEnableAppContainer[] = "enable-appcontainer";

// Allows shmat() system call in the GPU sandbox.
const char kGpuSandboxAllowSysVShm[] = "gpu-sandbox-allow-sysv-shm";

// Makes GPU sandbox failures fatal.
const char kGpuSandboxFailuresFatal[] = "gpu-sandbox-failures-fatal";

#if defined(OS_WIN)
// Allows third party modules to inject by disabling the BINARY_SIGNATURE
// mitigation policy on Win10+. Also has other effects in ELF.
const char kAllowThirdPartyModules[] = "allow-third-party-modules";
#endif

// Flags spied upon from other layers.
const char kGpuProcess[] = "gpu-process";
const char kPpapiBrokerProcess[] = "ppapi-broker";
const char kPpapiPluginProcess[] = "ppapi";
const char kRendererProcess[] = "renderer";
const char kUtilityProcess[] = "utility";
const char kDisableGpuSandbox[] = "disable-gpu-sandbox";
const char kNoSandbox[] = "no-sandbox";
#if defined(OS_WIN)
const char kNoSandboxAndElevatedPrivileges[] = "no-sandbox-and-elevated";
#endif
const char kEnableSandboxLogging[] = "enable-sandbox-logging";

}  // namespace switches

#if defined(OS_WIN)

bool IsWin32kLockdownEnabled() {
  return base::win::GetVersion() >= base::win::VERSION_WIN8 &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableWin32kLockDown);
}

#endif

}  // namespace service_manager
