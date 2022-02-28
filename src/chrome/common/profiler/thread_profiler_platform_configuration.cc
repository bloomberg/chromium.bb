// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/thread_profiler_platform_configuration.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "build/build_config.h"
#include "chrome/common/profiler/process_type.h"

#if defined(OS_ANDROID)
#include "chrome/android/modules/stack_unwinder/public/module.h"
#endif

namespace {

// The default configuration to use in the absence of special circumstances on a
// specific platform.
class DefaultPlatformConfiguration
    : public ThreadProfilerPlatformConfiguration {
 public:
  explicit DefaultPlatformConfiguration(bool browser_test_mode_enabled);

  // ThreadProfilerPlatformConfiguration:
  RuntimeModuleState GetRuntimeModuleState(
      absl::optional<version_info::Channel> release_channel) const override;

  RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const override;

  double GetChildProcessEnableFraction(
      metrics::CallStackProfileParams::Process process) const override;

  bool IsEnabledForThread(
      metrics::CallStackProfileParams::Process process,
      metrics::CallStackProfileParams::Thread thread) const override;

 protected:
  bool IsSupportedForChannel(
      absl::optional<version_info::Channel> release_channel) const override;

  bool browser_test_mode_enabled() const { return browser_test_mode_enabled_; }

 private:
  const bool browser_test_mode_enabled_;
};

DefaultPlatformConfiguration::DefaultPlatformConfiguration(
    bool browser_test_mode_enabled)
    : browser_test_mode_enabled_(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RuntimeModuleState
DefaultPlatformConfiguration::GetRuntimeModuleState(
    absl::optional<version_info::Channel> release_channel) const {
  return RuntimeModuleState::kModuleNotRequired;
}

ThreadProfilerPlatformConfiguration::RelativePopulations
DefaultPlatformConfiguration::GetEnableRates(
    absl::optional<version_info::Channel> release_channel) const {
  CHECK(IsSupportedForChannel(release_channel));

  if (!release_channel) {
    // This is a local/CQ build.
    return RelativePopulations{100, 0};
  }

  CHECK(*release_channel == version_info::Channel::CANARY ||
        *release_channel == version_info::Channel::DEV);

  return RelativePopulations{80, 20};
}

double DefaultPlatformConfiguration::GetChildProcessEnableFraction(
    metrics::CallStackProfileParams::Process process) const {
  DCHECK_NE(metrics::CallStackProfileParams::Process::kBrowser, process);

  switch (process) {
    case metrics::CallStackProfileParams::Process::kGpu:
    case metrics::CallStackProfileParams::Process::kNetworkService:
      return 1.0;

    case metrics::CallStackProfileParams::Process::kRenderer:
      // Run the profiler in all renderer processes if the browser test mode is
      // enabled, otherwise run in 20% of the processes to collect roughly as
      // many profiles for renderer processes as browser processes.
      return browser_test_mode_enabled() ? 1.0 : 0.2;

    default:
      return 0.0;
  }
}

bool DefaultPlatformConfiguration::IsEnabledForThread(
    metrics::CallStackProfileParams::Process process,
    metrics::CallStackProfileParams::Thread thread) const {
  // Enable for all supported threads.
  return true;
}

bool DefaultPlatformConfiguration::IsSupportedForChannel(
    absl::optional<version_info::Channel> release_channel) const {
  // The profiler is always supported for local builds and the CQ.
  if (!release_channel)
    return true;

  // Canary and dev are the only channels currently supported in release
  // builds.
  return *release_channel == version_info::Channel::CANARY ||
         *release_channel == version_info::Channel::DEV;
}

#if defined(OS_ANDROID)
// The configuration to use for the Android platform. Applies to ARM32 which is
// the only Android architecture currently supported by StackSamplingProfiler.
// Defined in terms of DefaultPlatformConfiguration where Android does not
// differ from the default case.
class AndroidPlatformConfiguration : public DefaultPlatformConfiguration {
 public:
  explicit AndroidPlatformConfiguration(bool browser_test_mode_enabled);

  // DefaultPlatformConfiguration:
  RuntimeModuleState GetRuntimeModuleState(
      absl::optional<version_info::Channel> release_channel) const override;

  RelativePopulations GetEnableRates(
      absl::optional<version_info::Channel> release_channel) const override;

  void RequestRuntimeModuleInstall() const override;

  double GetChildProcessEnableFraction(
      metrics::CallStackProfileParams::Process process) const override;

  bool IsEnabledForThread(
      metrics::CallStackProfileParams::Process process,
      metrics::CallStackProfileParams::Thread thread) const override;

 protected:
  bool IsSupportedForChannel(
      absl::optional<version_info::Channel> release_channel) const override;
};

AndroidPlatformConfiguration::AndroidPlatformConfiguration(
    bool browser_test_mode_enabled)
    : DefaultPlatformConfiguration(browser_test_mode_enabled) {}

ThreadProfilerPlatformConfiguration::RuntimeModuleState
AndroidPlatformConfiguration::GetRuntimeModuleState(
    absl::optional<version_info::Channel> release_channel) const {
  // The module will be present in releases due to having been installed via
  // RequestRuntimeModuleInstall(), and in local/CQ builds of bundle targets
  // where the module was installed with the bundle.
  if (stack_unwinder::Module::IsInstalled())
    return RuntimeModuleState::kModulePresent;

  if (release_channel) {
    // We only want to incur the cost of universally downloading the module in
    // early channels, where profiling will occur over substantially all of
    // the population. When supporting later channels in the future we will
    // enable profiling for only a fraction of users and only download for
    // those users.
    if (*release_channel == version_info::Channel::CANARY ||
        *release_channel == version_info::Channel::DEV) {
      return RuntimeModuleState::kModuleAbsentButAvailable;
    }

    return RuntimeModuleState::kModuleNotAvailable;
  }

  // This is a local or CQ build of a bundle where the module was not
  // installed with the bundle, or an apk where the module is not included.
  // The module is installable from the Play Store only for released Chrome so
  // is not available in this build.
  return RuntimeModuleState::kModuleNotAvailable;
}

ThreadProfilerPlatformConfiguration::RelativePopulations
AndroidPlatformConfiguration::GetEnableRates(
    absl::optional<version_info::Channel> release_channel) const {
  if (release_channel) {
    CHECK(*release_channel == version_info::Channel::CANARY ||
          *release_channel == version_info::Channel::DEV);
    // Use a 50/50 experiment to maximize signal in the relevant metrics.
    return RelativePopulations{0, 50};
  }

  return DefaultPlatformConfiguration::GetEnableRates(release_channel);
}

void AndroidPlatformConfiguration::RequestRuntimeModuleInstall() const {
  // The install can only be done in the browser process.
  CHECK_EQ(metrics::CallStackProfileParams::Process::kBrowser,
           GetProfileParamsProcess(*base::CommandLine::ForCurrentProcess()));

  // The install occurs asynchronously, with the module available at the first
  // run of Chrome following install.
  stack_unwinder::Module::RequestInstallation();
}

double AndroidPlatformConfiguration::GetChildProcessEnableFraction(
    metrics::CallStackProfileParams::Process process) const {
  // Profile all processes in browser test mode since they're disabled
  // otherwise.
  if (browser_test_mode_enabled())
    return DefaultPlatformConfiguration::GetChildProcessEnableFraction(process);

  if (process == metrics::CallStackProfileParams::Process::kRenderer)
    return 0.4;

  // TODO(https://crbug.com/1004855): Enable for all the default processes.
  return 0.0;
}

bool AndroidPlatformConfiguration::IsEnabledForThread(
    metrics::CallStackProfileParams::Process process,
    metrics::CallStackProfileParams::Thread thread) const {
  // Enable on renderer process main thread in production, for now.
  if (process == metrics::CallStackProfileParams::Process::kRenderer &&
      thread == metrics::CallStackProfileParams::Thread::kMain) {
    return true;
  }

  // Otherwise enable in dedicated ThreadProfiler browser tests.
  return browser_test_mode_enabled();
}

bool AndroidPlatformConfiguration::IsSupportedForChannel(
    absl::optional<version_info::Channel> release_channel) const {
  // Enable on canary, for now.
  if (release_channel && *release_channel == version_info::Channel::CANARY)
    return true;

  // Otherwise enable in dedicated ThreadProfiler browser tests.
  // TODO(https://crbug.com/1004855): Enable across all browser tests.
  return browser_test_mode_enabled();
}
#endif  // defined(OS_ANDROID)

}  // namespace

// static
std::unique_ptr<ThreadProfilerPlatformConfiguration>
ThreadProfilerPlatformConfiguration::Create(bool browser_test_mode_enabled) {
#if defined(OS_ANDROID)
  using PlatformConfiguration = AndroidPlatformConfiguration;
#else
  using PlatformConfiguration = DefaultPlatformConfiguration;
#endif
  return std::make_unique<PlatformConfiguration>(browser_test_mode_enabled);
}

bool ThreadProfilerPlatformConfiguration::IsSupported(
    absl::optional<version_info::Channel> release_channel) const {
  return base::StackSamplingProfiler::IsSupportedForCurrentPlatform() &&
         IsSupportedForChannel(release_channel);
}
