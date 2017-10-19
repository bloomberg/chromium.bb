// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/sandbox/sandbox.h"

#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#endif  // defined(OS_MACOSX)

namespace service_manager {

#if defined(OS_MACOSX)
bool Sandbox::Initialize(service_manager::SandboxType sandbox_type,
                         const base::FilePath& allowed_dir,
                         base::OnceClosure hook) {
  // Warm up APIs before turning on the sandbox.
  SandboxMac::Warmup(sandbox_type);

  // Execute the post warmup callback.
  if (!hook.is_null())
    std::move(hook).Run();

  // Actually sandbox the process.
  return SandboxMac::Enable(sandbox_type, allowed_dir);
}
#endif  // defined(OS_MACOSX)

}  // namespace service_manager
