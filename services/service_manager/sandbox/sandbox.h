// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICE_MANAGER_SANDBOX_SANDBOX_H_
#define SERVICE_MANAGER_SANDBOX_SANDBOX_H_

#include "build/build_config.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/sandbox_type.h"

#if defined(OS_LINUX)
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#endif

#if defined(OS_MACOSX)
#include "base/callback.h"
#include "base/files/file_path.h"
#endif  // defined(OS_MACOSX)

namespace sandbox {
struct SandboxInterfaceInfo;
}  // namespace sandbox

namespace service_manager {

// Interface to the service manager sandboxes across the various platforms.
//
// Ideally, this API would abstract away the platform differences, but there
// are some major OS differences that shape this interface, including:
// * Whether the sandboxing is performed by the launcher (Windows, Fuchsia
//   someday) or by the launchee (Linux, Mac).
// * The means of specifying the additional resources that are permitted.
// * The need to "warmup" other resources before engaing the sandbox.

class SERVICE_MANAGER_SANDBOX_EXPORT Sandbox {
 public:
#if defined(OS_LINUX)
  // These form a bitmask which describes the conditions of the Linux sandbox.
  // Note: this doesn't strictly give you the current status, it states
  // what will be enabled when the relevant processes are initialized.
  enum Status {
    // SUID sandbox active.
    kSUID = 1 << 0,

    // Sandbox is using a new PID namespace.
    kPIDNS = 1 << 1,

    // Sandbox is using a new network namespace.
    kNetNS = 1 << 2,

    // seccomp-bpf sandbox active.
    kSeccompBPF = 1 << 3,

    // The Yama LSM module is present and enforcing.
    kYama = 1 << 4,

    // seccomp-bpf sandbox is active and the kernel supports TSYNC.
    kSeccompTSYNC = 1 << 5,

    // User namespace sandbox active.
    kUserNS = 1 << 6,

    // A flag that denotes an invalid sandbox status.
    kInvalid = 1 << 31,
  };

  static bool Initialize(SandboxType sandbox_type,
                         SandboxLinux::PreSandboxHook hook,
                         const SandboxLinux::Options& options);
#endif  // defined(OS_LINUX)

#if defined(OS_MACOSX)
  // Initialize the sandbox of |sandbox_type|, permitting access to the one
  // directory specified by |allowed_dir| if non-empty. Runs |post_warmup_hook|
  // if non-empty after performing any sandbox warmup but immediately before
  // engaging the sandbox. Return true on success, false otherwise.
  static bool Initialize(SandboxType sandbox_type,
                         const base::FilePath& allowed_dir,
                         base::OnceClosure post_warmup_hook);
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  static bool Initialize(service_manager::SandboxType sandbox_type,
                         sandbox::SandboxInterfaceInfo* sandbox_info);
#endif  // defined(OS_WIN)
};

}  // namespace service_manager

#endif  // SERVICE_MANAGER_SANDBOX_SANDBOX_H_
