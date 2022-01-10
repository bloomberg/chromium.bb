// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/public/headless_shell.h"
#include "ui/gfx/switches.h"

#if defined(OS_MAC)
#include "chrome/app/chrome_main_mac.h"
#include "chrome/app/notification_metrics.h"
#endif

#if defined(OS_WIN) || defined(OS_LINUX)
#include "base/base_switches.h"
#endif

#if defined(OS_WIN)
#include "base/allocator/buildflags.h"
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/allocator_shim.h"
#endif

#include <timeapi.h>

#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/win/win_util.h"
#include "chrome/chrome_elf/chrome_elf_main.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/initialize_from_primary_module.h"
#include "chrome/install_static/install_details.h"

#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT int __cdecl ChromeMain(HINSTANCE instance,
                                 sandbox::SandboxInterfaceInfo* sandbox_info,
                                 int64_t exe_entry_point_ticks,
                                 base::PrefetchResultCode prefetch_result_code);
}
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
extern "C" {
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
__attribute__((visibility("default"))) int NO_STACK_PROTECTOR
ChromeMain(int argc, const char** argv);
}
#else
#error Unknown platform.
#endif

#if defined(OS_WIN)
DLLEXPORT int __cdecl ChromeMain(
    HINSTANCE instance,
    sandbox::SandboxInterfaceInfo* sandbox_info,
    int64_t exe_entry_point_ticks,
    base::PrefetchResultCode prefetch_result_code) {
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
int ChromeMain(int argc, const char** argv) {
  int64_t exe_entry_point_ticks = 0;
#else
#error Unknown platform.
#endif

#if defined(OS_WIN)
#if BUILDFLAG(USE_ALLOCATOR_SHIM) && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Call this early on in order to configure heap workarounds. This must be
  // called from chrome.dll. This may be a NOP on some platforms.
  base::allocator::ConfigurePartitionAlloc();
#endif

  base::UmaHistogramEnumeration("Windows.ChromeDllPrefetchResult",
                                prefetch_result_code);
  install_static::InitializeFromPrimaryModule();
#endif

  ChromeMainDelegate chrome_main_delegate(
      base::TimeTicks::FromInternalValue(exe_entry_point_ticks));
  content::ContentMainParams params(&chrome_main_delegate);

#if defined(OS_WIN)
  // The process should crash when going through abnormal termination, but we
  // must be sure to reset this setting when ChromeMain returns normally.
  auto crash_on_detach_resetter = base::ScopedClosureRunner(
      base::BindOnce(&base::win::SetShouldCrashOnProcessDetach,
                     base::win::ShouldCrashOnProcessDetach()));
  base::win::SetShouldCrashOnProcessDetach(true);
  base::win::SetAbortBehaviorForCrashReporting();
  params.instance = instance;
  params.sandbox_info = sandbox_info;

  // Pass chrome_elf's copy of DumpProcessWithoutCrash resolved via load-time
  // dynamic linking.
  base::debug::SetDumpWithoutCrashingFunction(&DumpProcessWithoutCrash);

  // Verify that chrome_elf and this module (chrome.dll and chrome_child.dll)
  // have the same version.
  if (install_static::InstallDetails::Get().VersionMismatch())
    base::debug::DumpWithoutCrashing();
#else
  params.argc = argc;
  params.argv = argv;
  base::CommandLine::Init(params.argc, params.argv);
#endif  // defined(OS_WIN)
  base::CommandLine::Init(0, nullptr);
  base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  ALLOW_UNUSED_LOCAL(command_line);

#if defined(OS_WIN)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kRaiseTimerFrequency)) {
    // Raise the timer interrupt frequency and leave it raised.
    timeBeginPeriod(1);
  }
#endif

#if defined(OS_MAC)
  SetUpBundleOverrides();
#endif

  // Start the sampling profiler as early as possible - namely, once the command
  // line data is available. Allocated as an object on the stack to ensure that
  // the destructor runs on shutdown, which is important to avoid the profiler
  // thread's destruction racing with main thread destruction.
  MainThreadStackSamplingProfiler scoped_sampling_profiler;

  // Chrome-specific process modes.
  if (headless::IsChromeNativeHeadless()) {
    headless::SetUpCommandLine(command_line);
  } else {
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) || \
    defined(OS_WIN)
    if (command_line->HasSwitch(switches::kHeadless)) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      command_line->AppendSwitch(::headless::switches::kEnableCrashReporter);
#endif
      return headless::HeadlessShellMain(std::move(params));
    }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MAC) ||
        // defined(OS_WIN)
  }

#if defined(OS_LINUX)
  // TODO(https://crbug.com/1176772): Remove when Chrome Linux is fully migrated
  // to Crashpad.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableCrashpad);
#endif

#if defined(OS_MAC)
  // Gracefully exit if the system tried to launch the macOS notification helper
  // app when a user clicked on a notification.
  if (IsAlertsHelperLaunchedViaNotificationAction()) {
    LogLaunchedViaNotificationAction(NotificationActionSource::kHelperApp);
    return 0;
  }
#endif

  int rv = content::ContentMain(std::move(params));

  return rv;
}
