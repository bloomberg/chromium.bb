// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_util.h"

#include <algorithm>
#include <cstdio>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_types.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/arc_features.h"
#include "components/exo/shell_surface_util.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/types/display_constants.h"

namespace arc {

namespace {

// This is for finch. See also crbug.com/633704 for details.
// TODO(hidehiko): More comments of the intention how this works, when
// we unify the commandline flags.
const base::Feature kEnableArcFeature{"EnableARC",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Possible values for --arc-availability flag.
constexpr char kAvailabilityNone[] = "none";
constexpr char kAvailabilityInstalled[] = "installed";
constexpr char kAvailabilityOfficiallySupported[] = "officially-supported";
constexpr char kAlwaysStartWithNoPlayStore[] =
    "always-start-with-no-play-store";

constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";

// ArcVmUreadaheadMode param value strings.
constexpr char kGenerate[] = "generate";
constexpr char kDisabled[] = "disabled";

// Do not run ureadahead in vm for devices with less than 8GB due to memory
// pressure issues since system will likely drop caches in this case.
// The value should match platform2/arc/vm/scripts/init/arcvm-ureadahead.conf
// in Chrome OS.
constexpr int kReadaheadTotalMinMemoryInKb = 7500000;

void SetArcCpuRestrictionCallback(
    login_manager::ContainerCpuRestrictionState state,
    bool success) {
  if (success)
    return;
  const char* message =
      (state == login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND)
          ? "unprioritize"
          : "prioritize";
  LOG(ERROR) << "Failed to " << message << " ARC";
}

void OnSetArcVmCpuRestriction(
    absl::optional<vm_tools::concierge::SetVmCpuRestrictionResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to call SetVmCpuRestriction";
    return;
  }
  if (!response->success())
    LOG(ERROR) << "SetVmCpuRestriction for ARCVM failed";
}

void SetArcVmCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  auto* client = chromeos::ConciergeClient::Get();
  if (!client) {
    LOG(ERROR) << "ConciergeClient is not available";
    return;
  }

  vm_tools::concierge::SetVmCpuRestrictionRequest request;
  request.set_cpu_cgroup(vm_tools::concierge::CPU_CGROUP_ARCVM);
  switch (cpu_restriction_state) {
    case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
      request.set_cpu_restriction_state(
          vm_tools::concierge::CPU_RESTRICTION_FOREGROUND);
      break;
    case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
      request.set_cpu_restriction_state(
          vm_tools::concierge::CPU_RESTRICTION_BACKGROUND);
      break;
  }

  client->SetVmCpuRestriction(request,
                              base::BindOnce(&OnSetArcVmCpuRestriction));
}

void SetArcContainerCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  if (!chromeos::SessionManagerClient::Get()) {
    LOG(WARNING) << "SessionManagerClient is not available";
    return;
  }

  login_manager::ContainerCpuRestrictionState state;
  switch (cpu_restriction_state) {
    case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
      state = login_manager::CONTAINER_CPU_RESTRICTION_FOREGROUND;
      break;
    case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
      state = login_manager::CONTAINER_CPU_RESTRICTION_BACKGROUND;
      break;
  }
  chromeos::SessionManagerClient::Get()->SetArcCpuRestriction(
      state, base::BindOnce(SetArcCpuRestrictionCallback, state));
}

// Decodes a job name that may have "_2d" e.g. |kArcCreateDataJobName|
// and returns a decoded string.
std::string DecodeJobName(const std::string& raw_job_name) {
  constexpr const char* kFind = "_2d";
  std::string decoded(raw_job_name);
  base::ReplaceSubstringsAfterOffset(&decoded, 0, kFind, "-");
  return decoded;
}

// Called when the Upstart operation started in ConfigureUpstartJobs is
// done. Handles the fatal error (if any) and then starts the next job.
void OnConfigureUpstartJobs(std::deque<JobDesc> jobs,
                            chromeos::VoidDBusMethodCallback callback,
                            bool result) {
  const std::string job_name = DecodeJobName(jobs.front().job_name);
  const bool is_start = (jobs.front().operation == UpstartOperation::JOB_START);

  if (!result && is_start) {
    LOG(ERROR) << "Failed to start " << job_name;
    // TODO(yusukes): Record UMA for this case.
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << job_name
          << (is_start ? " started" : (result ? " stopped " : " not running?"));
  jobs.pop_front();
  ConfigureUpstartJobs(std::move(jobs), std::move(callback));
}

}  // namespace

bool IsArcAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(chromeos::switches::kArcAvailability)) {
    const std::string value =
        command_line->GetSwitchValueASCII(chromeos::switches::kArcAvailability);
    DCHECK(value == kAvailabilityNone || value == kAvailabilityInstalled ||
           value == kAvailabilityOfficiallySupported)
        << "Unknown flag value: " << value;
    return value == kAvailabilityOfficiallySupported ||
           (value == kAvailabilityInstalled &&
            base::FeatureList::IsEnabled(kEnableArcFeature));
  }

  // For transition, fallback to old flags.
  // TODO(hidehiko): Remove this and clean up whole this function, when
  // session_manager supports a new flag.
  return command_line->HasSwitch(chromeos::switches::kEnableArc) ||
         (command_line->HasSwitch(chromeos::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kEnableArcFeature));
}

bool IsArcVmEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableArcVm);
}

bool IsArcVmRtVcpuEnabled(uint32_t cpus) {
  // TODO(kansho): remove switch after tast test use Finch instead.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableArcVmRtVcpu)) {
    return true;
  }
  if (cpus == 2 && base::FeatureList::IsEnabled(kRtVcpuDualCore))
    return true;
  if (cpus > 2 && base::FeatureList::IsEnabled(kRtVcpuQuadCore))
    return true;
  return false;
}

bool IsArcVmUseHugePages() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcVmUseHugePages);
}

bool IsArcVmDevConfIgnored() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kIgnoreArcVmDevConf);
}

ArcVmUreadaheadMode GetArcVmUreadaheadMode(SystemMemoryInfoCallback callback) {
  base::SystemMemoryInfoKB mem_info;
  DCHECK(callback);
  if (!callback.Run(&mem_info)) {
    LOG(ERROR) << "Failed to get system memory info";
    return ArcVmUreadaheadMode::DISABLED;
  }
  ArcVmUreadaheadMode mode = (mem_info.total > kReadaheadTotalMinMemoryInKb)
                                 ? ArcVmUreadaheadMode::READAHEAD
                                 : ArcVmUreadaheadMode::DISABLED;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kArcVmUreadaheadMode)) {
    const std::string value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            chromeos::switches::kArcVmUreadaheadMode);
    if (value == kGenerate) {
      mode = ArcVmUreadaheadMode::GENERATE;
    } else if (value == kDisabled) {
      mode = ArcVmUreadaheadMode::DISABLED;
    } else {
      LOG(ERROR) << "Invalid parameter " << value << " for "
                 << chromeos::switches::kArcVmUreadaheadMode;
    }
  }
  return mode;
}

bool ShouldArcAlwaysStart() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kArcStartMode))
    return false;
  return command_line->GetSwitchValueASCII(chromeos::switches::kArcStartMode) ==
         kAlwaysStartWithNoPlayStore;
}

bool ShouldArcAlwaysStartWithNoPlayStore() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             chromeos::switches::kArcStartMode) == kAlwaysStartWithNoPlayStore;
}

bool ShouldShowOptInForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcForceShowOptInUi);
}

void SetArcAlwaysStartWithoutPlayStoreForTesting() {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      chromeos::switches::kArcStartMode, kAlwaysStartWithNoPlayStore);
}

bool IsArcKioskAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(chromeos::switches::kArcAvailability)) {
    std::string value =
        command_line->GetSwitchValueASCII(chromeos::switches::kArcAvailability);
    if (value == kAvailabilityInstalled)
      return true;
    return IsArcAvailable();
  }

  // TODO(hidehiko): Remove this when session_manager supports the new flag.
  if (command_line->HasSwitch(chromeos::switches::kArcAvailable))
    return true;

  // If not special kiosk device case, use general ARC check.
  return IsArcAvailable();
}

void SetArcAvailableCommandLineForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(chromeos::switches::kArcAvailability,
                                  kAvailabilityOfficiallySupported);
}

bool IsArcKioskMode() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp();
}

bool IsRobotOrOfflineDemoAccountMode() {
  return user_manager::UserManager::IsInitialized() &&
         (user_manager::UserManager::Get()->IsLoggedInAsArcKioskApp() ||
          user_manager::UserManager::Get()->IsLoggedInAsPublicAccount());
}

bool IsArcAllowedForUser(const user_manager::User* user) {
  if (!user) {
    VLOG(1) << "No ARC for nullptr user.";
    return false;
  }

  // ARC is only supported for the following cases:
  // - Users have Gaia accounts;
  // - Active directory users;
  // - ARC kiosk session;
  // - Public Session users;
  //   USER_TYPE_ARC_KIOSK_APP check is compatible with IsArcKioskMode()
  //   above because ARC kiosk user is always the primary/active user of a
  //   user session. The same for USER_TYPE_PUBLIC_ACCOUNT.
  if (!user->HasGaiaAccount() && !user->IsActiveDirectoryUser() &&
      user->GetType() != user_manager::USER_TYPE_ARC_KIOSK_APP &&
      user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT) {
    VLOG(1) << "Users without GAIA or AD accounts, or not ARC kiosk apps are "
               "not supported in ARC.";
    return false;
  }

  return true;
}

bool IsArcOptInVerificationDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableArcOptInVerification);
}

int GetWindowTaskId(const aura::Window* window) {
  if (!window)
    return kNoTaskId;
  const std::string* arc_app_id = exo::GetShellApplicationId(window);
  if (!arc_app_id)
    return kNoTaskId;
  return GetTaskIdFromWindowAppId(*arc_app_id);
}

int GetTaskIdFromWindowAppId(const std::string& app_id) {
  int task_id;
  if (std::sscanf(app_id.c_str(), "org.chromium.arc.%d", &task_id) != 1)
    return kNoTaskId;
  return task_id;
}

void SetArcCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  // Ignore any calls to restrict the ARC container if the specified command
  // line flag is set.
  if (chromeos::switches::IsArcCpuRestrictionDisabled() &&
      cpu_restriction_state == CpuRestrictionState::CPU_RESTRICTION_BACKGROUND)
    return;

  if (IsArcVmEnabled()) {
    SetArcVmCpuRestriction(cpu_restriction_state);
  } else {
    SetArcContainerCpuRestriction(cpu_restriction_state);
  }
}

bool IsArcForceCacheAppIcon() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcGeneratePlayAutoInstall);
}

bool IsArcDataCleanupOnStartRequested() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcDataCleanupOnStart);
}

bool IsArcAppSyncFlowDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcDisableAppSync);
}

bool IsArcLocaleSyncDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcDisableLocaleSync);
}

bool IsArcPlayAutoInstallDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kArcDisablePlayAutoInstall);
}

int32_t GetLcdDensityForDeviceScaleFactor(float device_scale_factor) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kArcScale)) {
    const std::string dpi_str =
        command_line->GetSwitchValueASCII(chromeos::switches::kArcScale);
    int dpi;
    if (base::StringToInt(dpi_str, &dpi))
      return dpi;
    VLOG(1) << "Invalid Arc scale set. Using default.";
  }
  // TODO(b/131884992): Remove the logic to update default lcd density once
  // per-display-density is supported.
  constexpr float kEpsilon = 0.001;
  if (std::abs(device_scale_factor - display::kDsf_2_252) < kEpsilon)
    return 280;
  if (std::abs(device_scale_factor - 2.4f) < kEpsilon)
    return 280;
  if (std::abs(device_scale_factor - 1.6f) < kEpsilon)
    return 213;  // TVDPI
  if (std::abs(device_scale_factor - display::kDsf_1_777) < kEpsilon)
    return 240;  // HDPI
  if (std::abs(device_scale_factor - display::kDsf_1_8) < kEpsilon)
    return 240;  // HDPI
  if (std::abs(device_scale_factor - display::kDsf_2_666) < kEpsilon)
    return 320;  // XHDPI

  constexpr float kChromeScaleToAndroidScaleRatio = 0.75f;
  constexpr int32_t kDefaultDensityDpi = 160;
  return static_cast<int32_t>(
      std::max(1.0f, device_scale_factor * kChromeScaleToAndroidScaleRatio) *
      kDefaultDensityDpi);
}

int GetSystemPropertyInt(const std::string& property) {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, property}, &output))
    return -1;
  int output_int;
  return base::StringToInt(output, &output_int) ? output_int : -1;
}

JobDesc::JobDesc(const std::string& job_name,
                 UpstartOperation operation,
                 const std::vector<std::string>& environment)
    : job_name(job_name), operation(operation), environment(environment) {}

JobDesc::~JobDesc() = default;

JobDesc::JobDesc(const JobDesc& other) = default;

void ConfigureUpstartJobs(std::deque<JobDesc> jobs,
                          chromeos::VoidDBusMethodCallback callback) {
  if (jobs.empty()) {
    std::move(callback).Run(true);
    return;
  }

  if (jobs.front().operation == UpstartOperation::JOB_STOP_AND_START) {
    // Expand the restart operation into two, stop and start.
    jobs.front().operation = UpstartOperation::JOB_START;
    jobs.push_front({jobs.front().job_name, UpstartOperation::JOB_STOP,
                     jobs.front().environment});
  }

  const auto& job_name = jobs.front().job_name;
  const auto& operation = jobs.front().operation;
  const auto& environment = jobs.front().environment;

  VLOG(1) << (operation == UpstartOperation::JOB_START ? "Starting "
                                                       : "Stopping ")
          << DecodeJobName(job_name);

  auto wrapped_callback = base::BindOnce(&OnConfigureUpstartJobs,
                                         std::move(jobs), std::move(callback));
  switch (operation) {
    case UpstartOperation::JOB_START:
      chromeos::UpstartClient::Get()->StartJob(job_name, environment,
                                               std::move(wrapped_callback));
      break;
    case UpstartOperation::JOB_STOP:
      chromeos::UpstartClient::Get()->StopJob(job_name, environment,
                                              std::move(wrapped_callback));
      break;
    case UpstartOperation::JOB_STOP_AND_START:
      NOTREACHED();
      break;
  }
}

}  // namespace arc
