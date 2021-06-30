// Copyright 2013 The Chromium Embedded Framework Authors. Portions Copyright
// 2011 the Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "base/logging.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_factory.h"

#include "cef/libcef/features/features.h"
#include "include/cef_sandbox_win.h"

namespace {

// From content/app/startup_helper_win.cc:
void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* info) {
  info->broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!info->broker_services) {
    info->target_services = sandbox::SandboxFactory::GetTargetServices();
  } else {
    // Ensure the proper mitigations are enforced for the browser process.
    sandbox::ApplyProcessMitigationsToCurrentProcess(
        sandbox::MITIGATION_DEP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
        sandbox::MITIGATION_HARDEN_TOKEN_IL_POLICY);
    // Note: these mitigations are "post-startup".  Some mitigations that need
    // to be enabled sooner (e.g. MITIGATION_EXTENSION_POINT_DISABLE) are done
    // so in Chrome_ELF.
  }
}

}  // namespace

void* cef_sandbox_info_create() {
  sandbox::SandboxInterfaceInfo* info = new sandbox::SandboxInterfaceInfo();
  memset(info, 0, sizeof(sandbox::SandboxInterfaceInfo));
  InitializeSandboxInfo(info);
  return info;
}

void cef_sandbox_info_destroy(void* sandbox_info) {
  delete static_cast<sandbox::SandboxInterfaceInfo*>(sandbox_info);
}

#if BUILDFLAG(IS_CEF_SANDBOX_BUILD)
// Avoid bringing in absl dependencies.
namespace absl {

// From third_party/abseil-cpp/absl/types/bad_optional_access.cc
namespace optional_internal {
void throw_bad_optional_access() {
  LOG(FATAL) << "Bad optional access";
  abort();
}
}  // namespace optional_internal

// From third_party/abseil-cpp/absl/types/bad_variant_access.cc
namespace variant_internal {
void ThrowBadVariantAccess() {
  LOG(FATAL) << "Bad variant access";
  abort();
}
}  // namespace variant_internal

}  // namespace absl
#endif  // BUILDFLAG(IS_CEF_SANDBOX_BUILD)
