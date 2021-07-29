// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_util.h"

#include <utility>

#include "ash/constants/app_types.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_mime_types_service.h"
#include "chrome/browser/ash/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_crostini_tracker.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace crostini {

// We use an arbitrary well-formed extension id for the Terminal app, this
// is equal to GenerateId("Terminal").
const char kCrostiniDeletedTerminalId[] = "oajcgpnkmhaalajejhlfpacbiokdnnfe";
// web_app::GenerateAppId(/*manifest_id=*/absl::nullopt,
//     GURL("chrome-untrusted://terminal/html/terminal.html"))
const char kCrostiniTerminalSystemAppId[] = "fhicihalidkgcimdmhpohldehjmcabcf";

const char kCrostiniDefaultVmName[] = "termina";
const char kCrostiniDefaultContainerName[] = "penguin";
const char kCrostiniDefaultUsername[] = "emperor";
const char kCrostiniDefaultImageServerUrl[] =
    "https://storage.googleapis.com/cros-containers/%d";
const char kCrostiniStretchImageAlias[] = "debian/stretch";
const char kCrostiniBusterImageAlias[] = "debian/buster";
const char kCrostiniDlcName[] = "termina-dlc";

const base::FilePath::CharType kHomeDirectory[] = FILE_PATH_LITERAL("/home");

namespace {

constexpr char kCrostiniAppLaunchHistogram[] = "Crostini.AppLaunch";
constexpr char kCrostiniAppLaunchResultHistogram[] = "Crostini.AppLaunchResult";
constexpr char kCrostiniAppLaunchResultHistogramTerminal[] =
    "Crostini.AppLaunchResult.Terminal";
constexpr char kCrostiniAppLaunchResultHistogramRegistered[] =
    "Crostini.AppLaunchResult.Registered";
constexpr char kCrostiniAppLaunchResultHistogramUnknown[] =
    "Crostini.AppLaunchResult.Unknown";
constexpr int64_t kDelayBeforeSpinnerMs = 400;

void OnApplicationLaunched(const std::string& app_id,
                           crostini::CrostiniSuccessCallback callback,
                           const crostini::CrostiniResult failure_result,
                           bool success,
                           const std::string& failure_reason) {
  CrostiniAppLaunchAppType type = CrostiniAppLaunchAppType::kRegisteredApp;
  CrostiniResult result = success ? CrostiniResult::SUCCESS : failure_result;
  if (app_id == kCrostiniTerminalSystemAppId) {
    // Terminal usually takes care of its own metrics, but if we're recording
    // the result here we need to also record the launch.
    type = CrostiniAppLaunchAppType::kTerminal;
    RecordAppLaunchHistogram(type);
  }
  RecordAppLaunchResultHistogram(type, result);
  std::move(callback).Run(success, failure_reason);
}

void OnLaunchFailed(const std::string& app_id,
                    crostini::CrostiniSuccessCallback callback,
                    const std::string& failure_reason,
                    crostini::CrostiniResult result) {
  // Remove the spinner and icon. Controller doesn't exist in tests.
  // TODO(timloh): Consider also displaying a notification for failure.
  if (auto* chrome_controller = ChromeShelfController::instance()) {
    chrome_controller->GetShelfSpinnerController()->CloseSpinner(app_id);
  }
  OnApplicationLaunched(app_id, std::move(callback), result, false,
                        failure_reason);
}

void OnSharePathForLaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<std::string>& args,
    crostini::CrostiniSuccessCallback callback,
    bool success,
    const std::string& failure_reason) {
  if (!success) {
    return OnLaunchFailed(
        app_id, std::move(callback),
        "failed to share paths to launch " + app_id + ":" + failure_reason,
        CrostiniResult::SHARE_PATHS_FAILED);
  }
  const crostini::ContainerId container_id(registration.VmName(),
                                           registration.ContainerName());
  if (app_id == kCrostiniTerminalSystemAppId) {
    // Use first file as 'cwd'.
    std::string cwd = !args.empty() ? args[0] : "";
    LaunchTerminal(profile, display_id, container_id, cwd);
    return std::move(callback).Run(true, "");
  }
  crostini::CrostiniManager::GetForProfile(profile)->LaunchContainerApplication(
      container_id, registration.DesktopFileId(), args, registration.IsScaled(),
      base::BindOnce(OnApplicationLaunched, app_id, std::move(callback),
                     crostini::CrostiniResult::UNKNOWN_ERROR));
}

void LaunchApplication(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<LaunchArg>& args,
    crostini::CrostiniSuccessCallback callback) {
  ChromeShelfController* chrome_shelf_controller =
      ChromeShelfController::instance();
  DCHECK(chrome_shelf_controller);

  AppServiceAppWindowShelfController* app_service_controller =
      chrome_shelf_controller->app_service_app_window_controller();
  DCHECK(app_service_controller);

  AppServiceAppWindowCrostiniTracker* crostini_tracker =
      app_service_controller->app_service_crostini_tracker();
  DCHECK(crostini_tracker);

  crostini_tracker->OnAppLaunchRequested(app_id, display_id);

  auto* share_path = guest_os::GuestOsSharePath::GetForProfile(profile);
  const auto vm_name = registration.VmName();

  // Share any paths not in crostini.  The user will see the spinner while this
  // is happening.
  std::vector<base::FilePath> paths_to_share;
  std::vector<std::string> launch_args;
  launch_args.reserve(args.size());
  for (const auto& arg : args) {
    if (absl::holds_alternative<std::string>(arg)) {
      launch_args.push_back(absl::get<std::string>(arg));
      continue;
    }
    const storage::FileSystemURL& url = absl::get<storage::FileSystemURL>(arg);
    base::FilePath path;
    if (!file_manager::util::ConvertFileSystemURLToPathInsideCrostini(
            profile, url, &path)) {
      return OnLaunchFailed(
          app_id, std::move(callback),
          "Cannot share file with crostini: " + url.DebugString(),
          CrostiniResult::SHARE_PATHS_FAILED);
    }
    if (url.mount_filesystem_id() !=
            file_manager::util::GetCrostiniMountPointName(profile) &&
        !share_path->IsPathShared(vm_name, url.path())) {
      paths_to_share.push_back(url.path());
    }
    launch_args.push_back(path.value());
  }

  if (paths_to_share.empty()) {
    OnSharePathForLaunchApplication(profile, app_id, std::move(registration),
                                    display_id, std::move(launch_args),
                                    std::move(callback), true, "");
  } else {
    share_path->SharePaths(
        vm_name, std::move(paths_to_share), /*persist=*/false,
        base::BindOnce(OnSharePathForLaunchApplication, profile, app_id,
                       std::move(registration), display_id,
                       std::move(launch_args), std::move(callback)));
  }
}

}  // namespace

ContainerId::ContainerId(std::string vm_name,
                         std::string container_name) noexcept
    : vm_name(std::move(vm_name)), container_name(std::move(container_name)) {}

bool operator<(const ContainerId& lhs, const ContainerId& rhs) noexcept {
  const auto result = lhs.vm_name.compare(rhs.vm_name);
  return result < 0 || (result == 0 && lhs.container_name < rhs.container_name);
}

bool operator==(const ContainerId& lhs, const ContainerId& rhs) noexcept {
  return lhs.vm_name == rhs.vm_name && lhs.container_name == rhs.container_name;
}

std::ostream& operator<<(std::ostream& ostream,
                         const ContainerId& container_id) {
  return ostream << "(vm: \"" << container_id.vm_name << "\" container: \""
                 << container_id.container_name << "\")";
}

ContainerId ContainerId::GetDefault() {
  return ContainerId(kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
}

bool IsUninstallable(Profile* profile, const std::string& app_id) {
  if (!CrostiniFeatures::Get()->IsEnabled(profile) ||
      app_id == kCrostiniTerminalSystemAppId) {
    return false;
  }
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (registration)
    return registration->CanUninstall();
  return false;
}

bool IsCrostiniRunning(Profile* profile) {
  return crostini::CrostiniManager::GetForProfile(profile)->IsVmRunning(
      kCrostiniDefaultVmName);
}

bool ShouldConfigureDefaultContainer(Profile* profile) {
  const base::FilePath ansible_playbook_file_path =
      profile->GetPrefs()->GetFilePath(prefs::kCrostiniAnsiblePlaybookFilePath);
  bool default_container_configured = profile->GetPrefs()->GetBoolean(
      prefs::kCrostiniDefaultContainerConfigured);
  return base::FeatureList::IsEnabled(
             features::kCrostiniAnsibleInfrastructure) &&
         !default_container_configured && !ansible_playbook_file_path.empty();
}

bool ShouldAllowContainerUpgrade(Profile* profile) {
  return CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(profile) &&
         crostini::CrostiniManager::GetForProfile(profile)
             ->IsContainerUpgradeable(ContainerId(
                 kCrostiniDefaultVmName, kCrostiniDefaultContainerName));
}

void AddSpinner(crostini::CrostiniManager::RestartId restart_id,
                const std::string& app_id,
                Profile* profile) {
  ChromeShelfController* chrome_controller = ChromeShelfController::instance();
  if (chrome_controller &&
      crostini::CrostiniManager::GetForProfile(profile)->IsRestartPending(
          restart_id)) {
    chrome_controller->GetShelfSpinnerController()->AddSpinnerToShelf(
        app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  }
}

bool MaybeShowCrostiniDialogBeforeLaunch(Profile* profile,
                                         CrostiniResult result) {
  if (result == CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED ||
      result == CrostiniResult::LOAD_COMPONENT_FAILED) {
    ShowCrostiniUpdateComponentView(profile, CrostiniUISurface::kAppList);
    VLOG(1) << "Update Component dialog";
    return true;
  }
  return false;
}

void LaunchCrostiniAppImpl(
    Profile* profile,
    const std::string& app_id,
    guest_os::GuestOsRegistryService::Registration registration,
    int64_t display_id,
    const std::vector<LaunchArg>& args,
    CrostiniSuccessCallback callback) {
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  // Store these as we move |registration| into LaunchContainerApplication().
  const ContainerId container_id(registration.VmName(),
                                 registration.ContainerName());

  if (app_id == kCrostiniTerminalSystemAppId) {
    // If terminal is launched with a 'cwd' file, we may need to launch the VM
    // and share the path before launching terminal.
    bool requires_share = false;
    base::FilePath cwd;
    if (!args.empty() &&
        absl::holds_alternative<storage::FileSystemURL>(args[0])) {
      const storage::FileSystemURL& url =
          absl::get<storage::FileSystemURL>(args[0]);
      if (url.mount_filesystem_id() !=
          file_manager::util::GetCrostiniMountPointName(profile)) {
        requires_share = true;
      } else {
        file_manager::util::ConvertFileSystemURLToPathInsideCrostini(profile,
                                                                     url, &cwd);
      }
    }

    if (!requires_share) {
      LaunchTerminal(profile, display_id, container_id, cwd.value());
      return std::move(callback).Run(true, "");
    }
  } else {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kRegisteredApp);
  }

  // Update the last launched time and Termina version.
  registry_service->AppLaunched(app_id);
  crostini_manager->UpdateLaunchMetricsForEnterpriseReporting();

  auto restart_id = crostini_manager->RestartCrostini(
      container_id,
      base::BindOnce(
          [](Profile* profile, const std::string& app_id,
             guest_os::GuestOsRegistryService::Registration registration,
             int64_t display_id, const std::vector<LaunchArg> args,
             crostini::CrostiniSuccessCallback callback,
             crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              OnLaunchFailed(app_id, std::move(callback),
                             base::StringPrintf(
                                 "crostini restart to launch app %s failed: %d",
                                 app_id.c_str(), result),
                             result);
              if (crostini::MaybeShowCrostiniDialogBeforeLaunch(profile,
                                                                result)) {
                VLOG(1) << "Crostini restart blocked by dialog";
              }
              return;
            }

            LaunchApplication(profile, app_id, std::move(registration),
                              display_id, args, std::move(callback));
          },
          profile, app_id, std::move(registration), display_id, args,
          std::move(callback)));

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AddSpinner, restart_id, app_id, profile),
      base::TimeDelta::FromMilliseconds(kDelayBeforeSpinnerMs));
}

void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<LaunchArg>& args,
                       CrostiniSuccessCallback callback) {
  // Policies can change under us, and crostini may now be forbidden.
  std::string reason;
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile, &reason)) {
    LOG(ERROR) << "Crostini not allowed: " << reason;
    return std::move(callback).Run(false, "Crostini UI not allowed");
  }
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile);

  // At this point, we know that Crostini UI is allowed.
  if (app_id == kCrostiniTerminalSystemAppId &&
      !CrostiniFeatures::Get()->IsEnabled(profile)) {
    crostini::CrostiniInstaller::GetForProfile(profile)->ShowDialog(
        CrostiniUISurface::kAppList);
    return std::move(callback).Run(false, "Crostini not installed");
  }

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  absl::optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(app_id);
  if (!registration) {
    RecordAppLaunchHistogram(CrostiniAppLaunchAppType::kUnknownApp);
    RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType::kUnknownApp,
                                   CrostiniResult::UNREGISTERED_APPLICATION);
    return std::move(callback).Run(
        false, "LaunchCrostiniApp called with an unknown app_id: " + app_id);
  }

  if (crostini_manager->IsUncleanStartup()) {
    // Prompt for user-restart.
    return ShowCrostiniRecoveryView(
        profile, crostini::CrostiniUISurface::kAppList, app_id, display_id,
        args, std::move(callback));
  }

  if (crostini_manager->GetCrostiniDialogStatus(DialogType::UPGRADER)) {
    // Reshow the existing dialog.
    chromeos::CrostiniUpgraderDialog::Reshow();
    VLOG(1) << "Reshowing upgrade dialog";
    std::move(callback).Run(
        false, "LaunchCrostiniApp called while upgrade dialog showing");
    return;
  }
  LaunchCrostiniAppImpl(profile, app_id, std::move(*registration), display_id,
                        args, std::move(callback));
}

std::string CryptohomeIdForProfile(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  // Empty id means we're running in a test.
  return id.empty() ? "test" : id;
}

std::string DefaultContainerUserNameForProfile(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return kCrostiniDefaultUsername;
  }
  std::string username = user->GetAccountName(/*use_display_email=*/false);

  // For gmail accounts, dots are already stripped away in the canonical
  // username. But for other accounts (e.g. managedchrome), we need to do this
  // manually.
  std::string::size_type index;
  while ((index = username.find('.')) != std::string::npos) {
    username.erase(index, 1);
  }

  return username;
}

base::FilePath ContainerChromeOSBaseDirectory() {
  return base::FilePath("/mnt/chromeos");
}

void AddNewLxdContainerToPrefs(Profile* profile,
                               const ContainerId& container_id) {
  auto* pref_service = profile->GetPrefs();

  base::Value new_container(base::Value::Type::DICTIONARY);
  new_container.SetKey(prefs::kVmKey, base::Value(container_id.vm_name));
  new_container.SetKey(prefs::kContainerKey,
                       base::Value(container_id.container_name));
  new_container.SetIntKey(prefs::kContainerOsVersionKey,
                          static_cast<int>(ContainerOsVersion::kUnknown));

  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);
  updater->Append(std::move(new_container));
}

namespace {

bool MatchContainerDict(const base::Value& dict,
                        const ContainerId& container_id) {
  const std::string* vm_name = dict.FindStringKey(prefs::kVmKey);
  const std::string* container_name = dict.FindStringKey(prefs::kContainerKey);
  return (vm_name && *vm_name == container_id.vm_name) &&
         (container_name && *container_name == container_id.container_name);
}

}  // namespace

void RemoveLxdContainerFromPrefs(Profile* profile,
                                 const ContainerId& container_id) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, crostini::prefs::kCrostiniContainers);
  updater->EraseListIter(
      std::find_if(updater->GetList().begin(), updater->GetList().end(),
                   [&](const auto& dict) {
                     return MatchContainerDict(dict, container_id);
                   }));

  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
      ->ClearApplicationList(guest_os::GuestOsRegistryService::VmType::
                                 ApplicationList_VmType_TERMINA,
                             container_id.vm_name, container_id.container_name);
  CrostiniMimeTypesServiceFactory::GetForProfile(profile)->ClearMimeTypes(
      container_id.vm_name, container_id.container_name);
}

const base::Value* GetContainerPrefValue(Profile* profile,
                                         const ContainerId& container_id,
                                         const std::string& key) {
  const base::Value* containers =
      profile->GetPrefs()->GetList(crostini::prefs::kCrostiniContainers);
  if (!containers) {
    return nullptr;
  }
  for (const auto& dict : containers->GetList()) {
    if (MatchContainerDict(dict, container_id))
      return dict.FindKey(key);
  }
  return nullptr;
}

void UpdateContainerPref(Profile* profile,
                         const ContainerId& container_id,
                         const std::string& key,
                         base::Value value) {
  ListPrefUpdate updater(profile->GetPrefs(),
                         crostini::prefs::kCrostiniContainers);
  auto it = std::find_if(
      updater->GetList().begin(), updater->GetList().end(),
      [&](const auto& dict) { return MatchContainerDict(dict, container_id); });
  if (it != updater->GetList().end()) {
    it->SetKey(key, std::move(value));
  }
}

std::u16string GetTimeRemainingMessage(base::TimeTicks start, int percent) {
  // Only estimate once we've spent at least 3 seconds OR gotten 10% of the way
  // through.
  constexpr base::TimeDelta kMinTimeForEstimate =
      base::TimeDelta::FromSeconds(3);
  constexpr base::TimeDelta kTimeDeltaZero = base::TimeDelta::FromSeconds(0);
  constexpr int kMinPercentForEstimate = 10;
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  if ((elapsed >= kMinTimeForEstimate && percent > 0) ||
      (percent >= kMinPercentForEstimate && elapsed > kTimeDeltaZero)) {
    base::TimeDelta total_time_expected = (elapsed * 100) / percent;
    base::TimeDelta time_remaining = total_time_expected - elapsed;
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                  ui::TimeFormat::LENGTH_SHORT, time_remaining);
  } else {
    return l10n_util::GetStringUTF16(
        IDS_CROSTINI_NOTIFICATION_OPERATION_STARTING);
  }
}

const ContainerId& DefaultContainerId() {
  static const base::NoDestructor<ContainerId> container_id(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
  return *container_id;
}

bool IsCrostiniWindow(const aura::Window* window) {
  // TODO(crbug/1158644): Non-Crostini apps (borealis, ...) have also been
  // identifying as Crostini. For now they're less common, and as they become
  // more productionised they get their own app type (e.g. lacros), but at some
  // point we'll want to untangle these different types to e.g. avoid double
  // counting in usage metrics.
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::CROSTINI_APP);
}

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type) {
  base::UmaHistogramEnumeration(kCrostiniAppLaunchHistogram, app_type);
}

void RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType type,
                                    crostini::CrostiniResult reason) {
  // We record one histogram for everything, so we have data continuity as
  // that's the metric we had first, and we also break results down by launch
  // type.
  base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogram, reason);
  switch (type) {
    case CrostiniAppLaunchAppType::kTerminal:
      base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogramTerminal,
                                    reason);
      break;
    case CrostiniAppLaunchAppType::kRegisteredApp:
      base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogramRegistered,
                                    reason);
      break;
    case CrostiniAppLaunchAppType::kUnknownApp:
      base::UmaHistogramEnumeration(kCrostiniAppLaunchResultHistogramUnknown,
                                    reason);
      break;
  }
}

}  // namespace crostini
