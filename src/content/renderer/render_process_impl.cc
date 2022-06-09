// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_impl.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <mlang.h>
#include <objidl.h>
#endif

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "content/common/thread_pool_util.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_v8_features.h"
#include "v8/include/v8-initialization.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(ARCH_CPU_X86_64)
#include "v8/include/v8-wasm-trap-handler-posix.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/android/cpu_affinity_setter.h"
#endif

namespace {

void SetV8FlagIfFeature(const base::Feature& feature, const char* v8_flag) {
  if (base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfNotFeature(const base::Feature& feature, const char* v8_flag) {
  if (!base::FeatureList::IsEnabled(feature)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

void SetV8FlagIfHasSwitch(const char* switch_name, const char* v8_flag) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    v8::V8::SetFlagsFromString(v8_flag, strlen(v8_flag));
  }
}

std::unique_ptr<base::ThreadPoolInstance::InitParams>
GetThreadPoolInitParams() {
  constexpr int kMaxNumThreadsInForegroundPoolLowerBound = 3;
  return std::make_unique<base::ThreadPoolInstance::InitParams>(
      std::max(kMaxNumThreadsInForegroundPoolLowerBound,
               content::GetMinForegroundThreadsInRendererThreadPool()));
}

#if defined(DCHECK_IS_CONFIGURABLE)
void V8DcheckCallbackHandler(const char* file, int line, const char* message) {
  // TODO(siggi): Set a crash key or a breadcrumb so the fact that we hit a
  //     V8 DCHECK gets out in the crash report.
  ::logging::LogMessage(file, line, logging::LOGGING_DCHECK).stream()
      << message;
}
#endif  // defined(DCHECK_IS_CONFIGURABLE)

}  // namespace

namespace content {

RenderProcessImpl::RenderProcessImpl()
    : RenderProcess("Renderer", GetThreadPoolInitParams()) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

#if defined(DCHECK_IS_CONFIGURABLE)
  // Some official builds ship with DCHECKs compiled in. Failing DCHECKs then
  // are either fatal or simply log the error, based on a feature flag.
  // Make sure V8 follows suit by setting a Dcheck handler that forwards to
  // the Chrome base logging implementation.
  v8::V8::SetDcheckErrorHandler(&V8DcheckCallbackHandler);

  if (!base::FeatureList::IsEnabled(base::kDCheckIsFatalFeature)) {
    // These V8 flags default on in this build configuration. This triggers
    // additional verification and code generation, which both slows down V8,
    // and can lead to fatal CHECKs. Turn these flags down to get something
    // closer to V8s normal performance and behavior.
    constexpr char kDisabledFlags[] =
        "--noturbo_verify "
        "--noturbo_verify_allocation "
        "--nodebug_code";

    v8::V8::SetFlagsFromString(kDisabledFlags, sizeof(kDisabledFlags));
  }
#endif  // defined(DCHECK_IS_CONFIGURABLE)

  if (base::SysInfo::IsLowEndDevice()) {
    std::string optimize_flag("--optimize-for-size");
    v8::V8::SetFlagsFromString(optimize_flag.c_str(), optimize_flag.size());
  }

  SetV8FlagIfHasSwitch(switches::kDisableJavaScriptHarmonyShipping,
                       "--noharmony-shipping");
  SetV8FlagIfHasSwitch(switches::kJavaScriptHarmony, "--harmony");
  SetV8FlagIfHasSwitch(switches::kEnableExperimentalWebAssemblyFeatures,
                       "--wasm-staging");

  SetV8FlagIfHasSwitch(switches::kEnableUnsafeFastJSCalls,
                       "--turbo-fast-api-calls");

  SetV8FlagIfFeature(features::kV8VmFuture, "--future");
  SetV8FlagIfNotFeature(features::kV8VmFuture, "--no-future");

  SetV8FlagIfFeature(features::kWebAssemblyBaseline, "--liftoff");
  SetV8FlagIfNotFeature(features::kWebAssemblyBaseline, "--no-liftoff");

  SetV8FlagIfFeature(features::kWebAssemblyCodeProtection,
                     "--wasm-write-protect-code-memory");
  SetV8FlagIfNotFeature(features::kWebAssemblyCodeProtection,
                        "--no-wasm-write-protect-code-memory");

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(ARCH_CPU_X86_64)
  SetV8FlagIfFeature(features::kWebAssemblyCodeProtectionPku,
                     "--wasm-memory-protection-keys");
  SetV8FlagIfNotFeature(features::kWebAssemblyCodeProtectionPku,
                        "--no-wasm-memory-protection-keys");
#endif  // (defined(OS_LINUX) || defined(OS_CHROMEOS)) &&
        // defined(ARCH_CPU_X86_64)

  SetV8FlagIfFeature(features::kWebAssemblyLazyCompilation,
                     "--wasm-lazy-compilation");
  SetV8FlagIfNotFeature(features::kWebAssemblyLazyCompilation,
                        "--no-wasm-lazy-compilation");

  SetV8FlagIfFeature(features::kWebAssemblySimd, "--experimental-wasm-simd");
  SetV8FlagIfNotFeature(features::kWebAssemblySimd,
                        "--no-experimental-wasm-simd");

  SetV8FlagIfFeature(blink::features::kJSONModules,
                     "--harmony-import-assertions");

  constexpr char kAtomicsFlag[] = "--harmony-atomics";
  v8::V8::SetFlagsFromString(kAtomicsFlag, sizeof(kAtomicsFlag));

  bool enable_shared_array_buffer_unconditionally =
      base::FeatureList::IsEnabled(features::kSharedArrayBuffer);

#if defined(OS_ANDROID)
  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kBigLittleScheduling,
          features::kBigLittleSchedulingRenderMainBigParam, false)) {
    SetCpuAffinityForCurrentThread(base::CpuAffinityMode::kBigCoresOnly);
  }
#else
  // Bypass the SAB restriction for the Finch "kill switch".
  enable_shared_array_buffer_unconditionally =
      enable_shared_array_buffer_unconditionally ||
      base::FeatureList::IsEnabled(features::kSharedArrayBufferOnDesktop);

  // Bypass the SAB restriction when enabled by Enterprise Policy.
  if (!enable_shared_array_buffer_unconditionally &&
      command_line->HasSwitch(
          switches::kSharedArrayBufferUnrestrictedAccessAllowed)) {
    enable_shared_array_buffer_unconditionally = true;
    blink::WebRuntimeFeatures::EnableSharedArrayBufferUnrestrictedAccessAllowed(
        true);
  }
#endif

  // The following line enables V8 support for SharedArrayBuffer. Note that the
  // SharedArrayBuffer constructor will be added to every global object only if
  // the v8 flag `sharedarraybuffer-per-context` is disabled (cf. next block of
  // code).
  blink::WebV8Features::EnableSharedArrayBuffer();

  if (!enable_shared_array_buffer_unconditionally) {
    // It is still possible to enable SharedArrayBuffer per context using the
    // `SharedArrayBufferConstructorEnabledCallback`. This will be done if the
    // context is cross-origin isolated or if it opts in into the reverse origin
    // trial.
    constexpr char kSABPerContextFlag[] =
        "--enable-sharedarraybuffer-per-context";
    v8::V8::SetFlagsFromString(kSABPerContextFlag, sizeof(kSABPerContextFlag));
  }

  // The cross-origin-webassembly-module-sharing-allowed flag is used to pass
  // the kCrossOriginWebAssemblyModuleSharingEnabled enterprise policy from the
  // browser process to the renderer process. The feature flag
  // features::kCrossOriginWebAssemblyModuleSharingEnabled exists to allow a
  // potential finch trial to roll back the deprecation if necessary.
  if (command_line->HasSwitch(
          switches::kCrossOriginWebAssemblyModuleSharingAllowed) ||
      base::FeatureList::IsEnabled(
          features::kCrossOriginWebAssemblyModuleSharingEnabled)) {
    blink::WebRuntimeFeatures::EnableCrossOriginWebAssemblyModuleSharingAllowed(
        true);
  }

  // The display-capture-permissions-policy-allowed flag is used to pass
  // the kDisplayCapturePermissionsPolicyEnabled Enterprise policy from the
  // browser process to the renderer process. This switch should be enabled by
  // default for now, but after a few milestones that allow enterprises to fix
  // broken applications, this flag will be removed.
  // This switch will only be enabled by the Enterprise policy.
  if (command_line->HasSwitch(
          switches::kDisplayCapturePermissionsPolicyAllowed)) {
    blink::WebRuntimeFeatures::EnableDisplayCapturePermissionsPolicy(true);
  }

  SetV8FlagIfFeature(features::kWebAssemblyTiering, "--wasm-tier-up");
  SetV8FlagIfNotFeature(features::kWebAssemblyTiering, "--no-wasm-tier-up");

  SetV8FlagIfFeature(features::kWebAssemblyDynamicTiering,
                     "--wasm-dynamic-tiering");

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(ARCH_CPU_X86_64)
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    if (command_line->HasSwitch(switches::kEnableCrashpad) ||
        command_line->HasSwitch(switches::kEnableCrashReporter) ||
        command_line->HasSwitch(switches::kEnableCrashReporterForTesting)) {
      // The trap handler is set as the first chance handler for Crashpad or
      // Breakpad's signal handler.
      v8::V8::EnableWebAssemblyTrapHandler(/*use_v8_signal_handler=*/false);
    } else if (!command_line->HasSwitch(
                   switches::kDisableInProcessStackTraces)) {
      if (base::debug::SetStackDumpFirstChanceCallback(
              v8::TryHandleWebAssemblyTrapPosix)) {
        // Crashpad and Breakpad are disabled, but the in-process stack dump
        // handlers are enabled, so set the callback on the stack dump handlers.
        v8::V8::EnableWebAssemblyTrapHandler(/*use_v8_signal_handler=*/false);
      } else {
        // As the registration of the callback failed, we don't enable trap
        // handlers.
      }
    } else {
      // There is no signal handler yet, but it's okay if v8 registers one.
      v8::V8::EnableWebAssemblyTrapHandler(/*use_v8_signal_handler=*/true);
    }
  }
#endif
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    // On Windows we use the default trap handler provided by V8.
    bool use_v8_trap_handler = true;
    v8::V8::EnableWebAssemblyTrapHandler(use_v8_trap_handler);
  }
#endif
#if defined(OS_MAC) && (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))
  if (base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
    // On macOS, Crashpad uses exception ports to handle signals in a different
    // process. As we cannot just pass a callback to this other process, we ask
    // V8 to install its own signal handler to deal with WebAssembly traps.
    bool use_v8_signal_handler = true;
    v8::V8::EnableWebAssemblyTrapHandler(use_v8_signal_handler);
  }
#endif  // defined(OS_MAC) && defined(ARCH_CPU_X86_64)
}

RenderProcessImpl::~RenderProcessImpl() {
#ifndef NDEBUG
  int count = blink::WebFrame::InstanceCount();
  if (count)
    DLOG(ERROR) << "WebFrame LEAKED " << count << " TIMES";
#endif

  GetShutDownEvent()->Signal();
}

std::unique_ptr<RenderProcess> RenderProcessImpl::Create() {
  return base::WrapUnique(new RenderProcessImpl());
}

void RenderProcessImpl::AddRefProcess() {
  NOTREACHED();
}

void RenderProcessImpl::ReleaseProcess() {
  NOTREACHED();
}

}  // namespace content
