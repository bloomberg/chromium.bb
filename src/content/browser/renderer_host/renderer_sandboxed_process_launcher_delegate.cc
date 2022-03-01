// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_sandboxed_process_launcher_delegate.h"

#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

#if defined(OS_WIN)
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/security_level.h"
#include "third_party/blink/public/common/switches.h"
#endif

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/public/common/content_switches.h"
#include "content/public/common/zygote/zygote_handle.h"  // nogncheck
#endif

namespace content {

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
ZygoteHandle RendererSandboxedProcessLauncherDelegate::GetZygote() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
  if (!renderer_prefix.empty())
    return nullptr;
  return GetGenericZygote();
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_MAC)
bool RendererSandboxedProcessLauncherDelegate::EnableCpuSecurityMitigations() {
  return true;
}
#endif  // defined(OS_MAC)

sandbox::mojom::Sandbox
RendererSandboxedProcessLauncherDelegate::GetSandboxType() {
  return sandbox::mojom::Sandbox::kRenderer;
}

#if defined(OS_WIN)
RendererSandboxedProcessLauncherDelegateWin::
    RendererSandboxedProcessLauncherDelegateWin(base::CommandLine* cmd_line,
                                                bool is_jit_disabled)
    : renderer_code_integrity_enabled_(
          GetContentClient()->browser()->IsRendererCodeIntegrityEnabled()) {
  if (is_jit_disabled) {
    dynamic_code_can_be_disabled_ = true;
    return;
  }
  if (cmd_line->HasSwitch(blink::switches::kJavaScriptFlags)) {
    std::string js_flags =
        cmd_line->GetSwitchValueASCII(blink::switches::kJavaScriptFlags);
    std::vector<base::StringPiece> js_flag_list = base::SplitStringPiece(
        js_flags, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& js_flag : js_flag_list) {
      if (js_flag == "--jitless") {
        // If v8 is running jitless then there is no need for the ability to
        // mark writable pages as executable to be available to the process.
        dynamic_code_can_be_disabled_ = true;
        break;
      }
    }
  }
}

bool RendererSandboxedProcessLauncherDelegateWin::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  sandbox::policy::SandboxWin::AddBaseHandleClosePolicy(policy);

  const std::wstring& sid =
      GetContentClient()->browser()->GetAppContainerSidForSandboxType(
          GetSandboxType());
  if (!sid.empty())
    sandbox::policy::SandboxWin::AddAppContainerPolicy(policy, sid.c_str());

  ContentBrowserClient::ChildSpawnFlags flags(
      ContentBrowserClient::ChildSpawnFlags::NONE);
  if (renderer_code_integrity_enabled_) {
    flags = ContentBrowserClient::ChildSpawnFlags::RENDERER_CODE_INTEGRITY;

    // If the renderer process is protected by code integrity, more
    // mitigations become available.
    if (dynamic_code_can_be_disabled_) {
      sandbox::MitigationFlags mitigation_flags =
          policy->GetDelayedProcessMitigations();
      mitigation_flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
      if (sandbox::SBOX_ALL_OK !=
          policy->SetDelayedProcessMitigations(mitigation_flags)) {
        return false;
      }
    }
  }

  return GetContentClient()->browser()->PreSpawnChild(
      policy, sandbox::mojom::Sandbox::kRenderer, flags);
}

bool RendererSandboxedProcessLauncherDelegateWin::CetCompatible() {
  // Disable CET for renderer because v8 deoptimization swaps stacks in a
  // non-compliant way. CET can be enabled where the renderer is known to
  // be jitless.
  return dynamic_code_can_be_disabled_;
}

#endif  // defined(OS_WIN)

}  // namespace content