// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include <fcntl.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/crosapi/browser_service_host_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/ash/crosapi/test_mojo_connection_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/common/channel_info.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/startup/startup_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

// TODO(crbug.com/1101667): Currently, this source has log spamming
// by LOG(WARNING) for non critical errors to make it easy
// to debug and develop. Get rid of the log spamming
// when it gets stable enough.

namespace crosapi {

namespace {

// The actual Lacros launch mode.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LacrosLaunchMode {
  // Indicates that Lacros is disabled.
  kLacrosDisabled = 0,
  // Indicates that Lacros and Ash are both enabled and accessible by the user.
  kSideBySide = 1,
  // Similar to kSideBySide but Lacros is the primary browser.
  kLacrosPrimary = 2,
  // Lacros is the only browser and Ash is disabled.
  kLacrosOnly = 3,
  kMaxValue = kLacrosOnly
};

using LaunchParamsFromBackground = BrowserManager::LaunchParamsFromBackground;

// Pointer to the global instance of BrowserManager.
BrowserManager* g_instance = nullptr;

constexpr char kLacrosCannotLaunchNotificationID[] =
    "lacros_cannot_launch_notification_id";
constexpr char kLacrosLauncherNotifierID[] = "lacros_launcher";

// To be sure the lacros is running with neutral priority
class ThreadPriorityDelegate : public base::LaunchOptions::PreExecDelegate {
 public:
  void RunAsyncSafe() override {
    base::PlatformThread::SetCurrentThreadPriority(
        base::ThreadPriority::NORMAL);
  }
};

base::FilePath LacrosLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log");
}

base::FilePath LacrosPreviousLogPath() {
  return browser_util::GetUserDataDir().Append("lacros.log.PREVIOUS");
}

// Moves any existing lacros log file to lacros.log.PREVIOUS. Returns true if a
// log file existed before being moved, and false if no log file was found.
bool RotateLacrosLogs() {
  // Remove lacros.log.PREVIOUS log entry if present.
  base::FilePath previous_log_path = LacrosPreviousLogPath();
  unlink(previous_log_path.value().c_str());

  base::FilePath log_path = LacrosLogPath();
  // Handle edge case where previous code created a symbolic link that could not
  // correctly resolve by deleting that symbolic link.
  if (base::IsLink(log_path)) {
    unlink(log_path.value().c_str());
    return true;
  }

  // If there is an existing log entry rename it to lacros.log.PREVIOUS.
  if (base::PathExists(log_path)) {
    base::Move(log_path, previous_log_path);
    return true;
  }

  return false;
}

// This method runs some work on a background thread prior to launching lacros.
// The returns struct is used by the main thread as parameters to launch Lacros.
LaunchParamsFromBackground DoLacrosBackgroundWorkPreLaunch(
    base::FilePath lacros_dir,
    bool cleared_user_data_dir) {
  LaunchParamsFromBackground params;

  // TODO(crbug/1198528): remove use_new_account_manager parameter.
  // This code wipes the Lacros --user-data-dir exactly once due to an
  // incompatible account_manager change. This code can be removed when ash is
  // newer than M92, as we can then assume that all relevant users have been
  // migrated.
  //
  // If we want to use the new account manager, and we haven't yet cleared the
  // user data dir, do so.
  if (!cleared_user_data_dir) {
    params.use_new_account_manager =
        base::DeletePathRecursively(browser_util::GetUserDataDir());
  } else {
    params.use_new_account_manager = true;
  }

  if (!RotateLacrosLogs()) {
    // If log file does not exist, most likely the user directory does not
    // exist either. So create it here.
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(browser_util::GetUserDataDir(),
                                          &error)) {
      LOG(ERROR) << "Failed to make directory "
                 << browser_util::GetUserDataDir()
                 << base::File::ErrorToString(error);
      return params;
    }
  }

  int fd = HANDLE_EINTR(
      open(LacrosLogPath().value().c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644));

  if (fd < 0) {
    PLOG(ERROR) << "Failed to get file descriptor for " << LacrosLogPath();
    return params;
  }

  params.logfd = base::ScopedFD(fd);
  return params;
}

std::string GetXdgRuntimeDir() {
  // If ash-chrome was given an environment variable, use it.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::string xdg_runtime_dir;
  if (env->GetVar("XDG_RUNTIME_DIR", &xdg_runtime_dir))
    return xdg_runtime_dir;

  // Otherwise provide the default for Chrome OS devices.
  return "/run/chrome";
}

void TerminateLacrosChrome(base::Process process) {
  // Here, lacros-chrome process may crashed, or be in the shutdown procedure.
  // Give some amount of time for the collection. In most cases,
  // this wait captures the process termination.
  constexpr base::TimeDelta kGracefulShutdownTimeout = base::Seconds(5);
  if (process.WaitForExitWithTimeout(kGracefulShutdownTimeout, nullptr))
    return;

  // Here, the process is not yet terminated.
  // This happens if some critical error happens on the mojo connection,
  // while both ash-chrome and lacros-chrome are still alive.
  // Terminate the lacros-chrome.
  bool success = process.Terminate(/*exit_code=*/0, /*wait=*/true);
  LOG_IF(ERROR, !success) << "Failed to terminate the lacros-chrome.";
}

void SetLaunchOnLoginPref(bool launch_on_login) {
  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetBoolean(
      browser_util::kLaunchOnLoginPref, launch_on_login);
}

bool GetLaunchOnLoginPref() {
  return ProfileManager::GetPrimaryUserProfile()->GetPrefs()->GetBoolean(
      browser_util::kLaunchOnLoginPref);
}

// Returns the initial browser action. No browser will be opened when
// lacros-chrome is initialized in the web Kiosk session.
browser_util::InitialBrowserAction GetInitialBrowserAction() {
  return browser_util::InitialBrowserAction(
      user_manager::UserManager::Get()->IsLoggedInAsWebKioskApp()
          ? mojom::InitialBrowserAction::kDoNotOpenWindow
          : mojom::InitialBrowserAction::kUseStartupPreference);
}

}  // namespace

// To be sure the lacros is running with neutral priority.
class LacrosThreadPriorityDelegate
    : public base::LaunchOptions::PreExecDelegate {
 public:
  void RunAsyncSafe() override {
    // SetCurrentThreadPriority() needs file I/O on /proc and /sys.
    base::ScopedAllowBlocking allow_blocking;
    base::PlatformThread::SetCurrentThreadPriority(
        base::ThreadPriority::NORMAL);
  }
};

// static
BrowserManager* BrowserManager::Get() {
  return g_instance;
}

BrowserManager::BrowserManager(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : component_manager_(manager),
      environment_provider_(std::make_unique<EnvironmentProvider>()) {
  DCHECK(!g_instance);
  g_instance = this;

  // Wait to query the flag until the user has entered the session. Enterprise
  // devices restart Chrome during login to apply flags. We don't want to run
  // the flag-off cleanup logic until we know we have the final flag state.
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->AddObserver(this);

  // CrosapiManager may not be initialized on unit testing.
  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->AddObserver(this);
  }

  std::string socket_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLacrosMojoSocketForTesting);
  if (!socket_path.empty()) {
    test_mojo_connection_manager_ =
        std::make_unique<crosapi::TestMojoConnectionManager>(
            base::FilePath(socket_path), environment_provider_.get());
  }
}

BrowserManager::~BrowserManager() {
  if (CrosapiManager::IsInitialized()) {
    CrosapiManager::Get()
        ->crosapi_ash()
        ->browser_service_host_ash()
        ->RemoveObserver(this);
  }

  // Unregister, just in case the manager is destroyed before
  // OnUserSessionStarted() is called.
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);

  // Try to kill the lacros-chrome binary.
  if (lacros_process_.IsValid())
    lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool BrowserManager::IsReady() const {
  return state_ != State::NOT_INITIALIZED && state_ != State::MOUNTING &&
         state_ != State::UNAVAILABLE;
}

bool BrowserManager::IsRunning() const {
  return state_ == State::RUNNING;
}

bool BrowserManager::IsRunningOrWillRun() const {
  return state_ == State::RUNNING || state_ == State::STARTING ||
         state_ == State::CREATING_LOG_FILE || state_ == State::TERMINATING;
}

void BrowserManager::SetLoadCompleteCallback(LoadCompleteCallback callback) {
  // We only support one client waiting.
  DCHECK(!load_complete_callback_);
  load_complete_callback_ = std::move(callback);
}

void BrowserManager::NewWindow(bool incognito) {
  // Chrome OS uses different user model where clicking the chrome icon always
  // opens a new tab page, and it doesn't matter whether lacros is launching
  // for the first time or not.
  auto result = MaybeStart(browser_util::InitialBrowserAction(
      incognito ? mojom::InitialBrowserAction::kOpenIncognitoWindow
                : mojom::InitialBrowserAction::kOpenNewTabPageWindow));
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  browser_service_->service->NewWindow(incognito, base::DoNothing());
}

bool BrowserManager::NewWindowForDetachingTabSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::
                 kNewWindowForDetachingTabMinVersion;
}

void BrowserManager::NewWindowForDetachingTab(
    const std::u16string& tab_id_str,
    const std::u16string& group_id_str,
    NewWindowForDetachingTabCallback callback) {
  // Chrome OS uses different user model where clicking the chrome icon always
  // opens a new tab page, and it doesn't matter whether lacros is launching
  // for the first time or not.
  auto result = MaybeStart(browser_util::InitialBrowserAction(
      mojom::InitialBrowserAction::kOpenNewTabPageWindow));
  if (result != MaybeStartResult::kRunning) {
    std::move(callback).Run(mojom::CreationResult::kBrowserNotRunning,
                            std::string() /*new_window*/);
    return;
  }

  if (!browser_service_.has_value()) {
    std::move(callback).Run(mojom::CreationResult::kServiceDisconnected,
                            std::string() /*new_window*/);
    return;
  }

  if (!NewWindowForDetachingTabSupported()) {
    std::move(callback).Run(mojom::CreationResult::kUnsupported,
                            std::string() /*new_window*/);
    return;
  }
  browser_service_->service->NewWindowForDetachingTab(tab_id_str, group_id_str,
                                                      std::move(callback));
}

bool BrowserManager::NewFullscreenWindowSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kNewFullscreenWindowMinVersion;
}

void BrowserManager::NewFullscreenWindow(const GURL& url,
                                         NewFullscreenWindowCallback callback) {
  auto result = MaybeStart(browser_util::InitialBrowserAction(
      mojom::InitialBrowserAction::kDoNotOpenWindow));
  if (result != MaybeStartResult::kRunning) {
    std::move(callback).Run(mojom::CreationResult::kBrowserNotRunning);
    return;
  }

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    std::move(callback).Run(mojom::CreationResult::kServiceDisconnected);
    return;
  }

  if (!NewFullscreenWindowSupported()) {
    std::move(callback).Run(mojom::CreationResult::kUnsupported);
    return;
  }
  browser_service_->service->NewFullscreenWindow(url, std::move(callback));
}

void BrowserManager::NewTab() {
  auto result = MaybeStart(browser_util::InitialBrowserAction(
      mojom::InitialBrowserAction::kOpenNewTabPageWindow));
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  browser_service_->service->NewTab(base::DoNothing());
}

void BrowserManager::OpenUrl(const GURL& url) {
  auto result = MaybeStart(browser_util::InitialBrowserAction(
      mojom::InitialBrowserAction::kOpenWindowWithUrls, {url}));
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  if (browser_service_->interface_version <
      mojom::BrowserService::kOpenUrlMinVersion) {
    LOG(ERROR) << "BrowserService does not support OpenUrl";
    return;
  }
  browser_service_->service->OpenUrl(url, base::DoNothing());
}

void BrowserManager::RestoreTab() {
  auto result = MaybeStart(browser_util::InitialBrowserAction(
      mojom::InitialBrowserAction::kRestoreLastSession));
  if (result != MaybeStartResult::kRunning)
    return;

  if (!browser_service_.has_value()) {
    LOG(ERROR) << "BrowserService was disconnected";
    return;
  }
  browser_service_->service->RestoreTab(base::DoNothing());
}

void BrowserManager::InitializeAndStart() {
  DCHECK_EQ(state_, State::NOT_INITIALIZED);

  // Perform the UMA recording for the current Lacros mode of operation.
  RecordLacrosLaunchMode();

  // Ensure this isn't run multiple times.
  session_manager::SessionManager::Get()->RemoveObserver(this);

  // May be null in tests.
  if (!component_manager_)
    return;

  PrepareLacrosPolicies();

  DCHECK(!browser_loader_);
  browser_loader_ = std::make_unique<BrowserLoader>(component_manager_);

  // Must be checked after user session start because it depends on user type.
  if (browser_util::IsLacrosEnabled()) {
    SetState(State::MOUNTING);
    browser_loader_->Load(base::BindOnce(&BrowserManager::OnLoadComplete,
                                         weak_factory_.GetWeakPtr()));
  } else {
    SetState(State::UNAVAILABLE);
    browser_loader_->Unload();
  }

  // Post `DryRunToCollectUMA()` to send UMA stats about sizes of files/dirs
  // inside the profile data directory.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ash::BrowserDataMigrator::DryRunToCollectUMA,
                     ProfileManager::GetPrimaryUserProfile()->GetPath()));
}

bool BrowserManager::GetFeedbackDataSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetFeedbackDataMinVersion;
}

void BrowserManager::GetFeedbackData(GetFeedbackDataCallback callback) {
  DCHECK(GetFeedbackDataSupported());
  browser_service_->service->GetFeedbackData(std::move(callback));
}

bool BrowserManager::GetHistogramsSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetHistogramsMinVersion;
}

void BrowserManager::GetHistograms(GetHistogramsCallback callback) {
  DCHECK(GetHistogramsSupported());
  browser_service_->service->GetHistograms(std::move(callback));
}

bool BrowserManager::GetActiveTabUrlSupported() const {
  return browser_service_.has_value() &&
         browser_service_->interface_version >=
             crosapi::mojom::BrowserService::kGetActiveTabUrlMinVersion;
}

void BrowserManager::GetActiveTabUrl(GetActiveTabUrlCallback callback) {
  DCHECK(GetActiveTabUrlSupported());
  browser_service_->service->GetActiveTabUrl(std::move(callback));
}

void BrowserManager::AddObserver(BrowserManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserManager::RemoveObserver(BrowserManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BrowserManager::Shutdown() {
  // Lacros KeepAlive should be disabled once Shutdown() has been signalled.
  // Further calls to `UpdateKeepAliveInBrowserIfNecessary()` will no-op after
  // `shutdown_requested_` has been set.
  UpdateKeepAliveInBrowserIfNecessary(false);
  shutdown_requested_ = true;

  // The lacros-chrome process may have already been terminated as the result of
  // a previous mojo pipe disconnection in `OnMojoDisconnected()` and has not
  // yet been restarted. Ensure the lacros process is still valid before
  // proceeding.
  if (!lacros_process_.IsValid())
    return;

  // Signal the the lacros process to terminate. This will result in mojo
  // disconnecting and a callback into `OnMojoDisconnected()`. This will post a
  // task that waits for a successful lacros-chrome exit on a separate thread.
  lacros_process_.Terminate(/*ignored=*/0, /*wait=*/false);
}

void BrowserManager::SetState(State state) {
  if (state_ == state)
    return;
  state_ = state;

  for (auto& observer : observers_) {
    if (state == State::TERMINATING) {
      observer.OnMojoDisconnected();
    }
    observer.OnStateChanged();
  }

  LaunchForKeepAliveIfNecessary();
}

BrowserManager::ScopedKeepAlive::~ScopedKeepAlive() {
  manager_->StopKeepAlive(feature_);
}

BrowserManager::ScopedKeepAlive::ScopedKeepAlive(BrowserManager* manager,
                                                 Feature feature)
    : manager_(manager), feature_(feature) {
  manager_->StartKeepAlive(feature_);
}

std::unique_ptr<BrowserManager::ScopedKeepAlive> BrowserManager::KeepAlive(
    Feature feature) {
  // Using new explicitly because ScopedKeepAlive's constructor is private.
  return base::WrapUnique(new ScopedKeepAlive(this, feature));
}

BrowserManager::BrowserServiceInfo::BrowserServiceInfo(
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* service,
    uint32_t interface_version)
    : mojo_id(mojo_id),
      service(service),
      interface_version(interface_version) {}

BrowserManager::BrowserServiceInfo::BrowserServiceInfo(
    const BrowserServiceInfo&) = default;
BrowserManager::BrowserServiceInfo&
BrowserManager::BrowserServiceInfo::operator=(const BrowserServiceInfo&) =
    default;
BrowserManager::BrowserServiceInfo::~BrowserServiceInfo() = default;

BrowserManager::MaybeStartResult BrowserManager::MaybeStart(
    browser_util::InitialBrowserAction initial_browser_action) {
  if (!browser_util::IsLacrosEnabled())
    return MaybeStartResult::kNotStarted;

  if (!browser_util::IsLacrosAllowedToLaunch()) {
    std::unique_ptr<message_center::Notification> notification =
        ash::CreateSystemNotification(
            message_center::NOTIFICATION_TYPE_SIMPLE,
            kLacrosCannotLaunchNotificationID,
            /*title=*/std::u16string(),
            l10n_util::GetStringUTF16(
                IDS_LACROS_CANNOT_LAUNCH_MULTI_SIGNIN_MESSAGE),
            /* display_source= */ std::u16string(), GURL(),
            message_center::NotifierId(
                message_center::NotifierType::SYSTEM_COMPONENT,
                kLacrosLauncherNotifierID),
            message_center::RichNotificationData(),
            base::MakeRefCounted<
                message_center::HandleNotificationClickDelegate>(
                base::RepeatingClosure()),
            gfx::kNoneIcon,
            message_center::SystemNotificationWarningLevel::NORMAL);

    SystemNotificationHelper::GetInstance()->Display(*notification);
    return MaybeStartResult::kNotStarted;
  }

  if (!IsReady()) {
    LOG(WARNING) << "lacros component image not yet available";
    return MaybeStartResult::kNotStarted;
  }
  DCHECK(!lacros_path_.empty());
  DCHECK(lacros_selection_.has_value());

  if (shutdown_requested_) {
    LOG(WARNING) << "lacros-chrome is preparing for system shutdown";
    return MaybeStartResult::kNotStarted;
  }

  if (state_ == State::TERMINATING) {
    LOG(WARNING) << "lacros-chrome is terminating, so cannot start now";
    return MaybeStartResult::kNotStarted;
  }

  if (state_ == State::CREATING_LOG_FILE || state_ == State::STARTING) {
    LOG(WARNING) << "lacros-chrome is in the process of launching";
    return MaybeStartResult::kStarting;
  }

  if (state_ == State::STOPPED) {
    // If lacros-chrome is not running, launch it.
    Start(std::move(initial_browser_action));
    return MaybeStartResult::kStarting;
  }

  return MaybeStartResult::kRunning;
}

void BrowserManager::Start(
    browser_util::InitialBrowserAction initial_browser_action) {
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(!lacros_path_.empty());
  DCHECK(!shutdown_requested_);
  // Ensure we're not trying to open a window before the shelf is initialized.
  DCHECK(ChromeShelfController::instance());

  // Always reset |relaunch_requested_| when launching Lacros.
  relaunch_requested_ = false;
  SetState(State::CREATING_LOG_FILE);

  // TODO(ythjkt): After M92 cherry-pick, clean up the following code by moving
  // the data wipe check logic from `BrowserDataMigrator` to browser_util.
  const std::string user_id_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          ProfileManager::GetPrimaryUserProfile());
  // Check if user data directory needs to be wiped for a backward incompatible
  // update.
  bool cleared_user_data_dir = !browser_util::IsDataWipeRequired(user_id_hash);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DoLacrosBackgroundWorkPreLaunch, lacros_path_,
                     cleared_user_data_dir),
      base::BindOnce(&BrowserManager::StartWithLogFile,
                     weak_factory_.GetWeakPtr(),
                     std::move(initial_browser_action)));
}

void BrowserManager::StartWithLogFile(
    browser_util::InitialBrowserAction initial_browser_action,
    LaunchParamsFromBackground params) {
  DCHECK_EQ(state_, State::CREATING_LOG_FILE);

  if (shutdown_requested_) {
    // StartWithLogFile() may have been posted before Shutdown() has been
    // signalled by the system. Ensure that we do not start lacros-chrome in
    // this case.
    LOG(ERROR) << "Start attempted after Shutdown() called.";
    SetState(State::STOPPED);
    return;
  }

  if (!params.use_new_account_manager) {
    // If `use_new_account_manager` is false, that means deleting old lacros
    // data directory failed. In such a case, do not launch lacros.
    LOG(ERROR) << "Failed to delete old user data dir.";
    SetState(State::STOPPED);
    return;
  }

  const std::string user_id_hash =
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          ProfileManager::GetPrimaryUserProfile());
  crosapi::browser_util::RecordDataVer(g_browser_process->local_state(),
                                       user_id_hash,
                                       version_info::GetVersion());

  std::string chrome_path = lacros_path_.MaybeAsASCII() + "/chrome";
  LOG(WARNING) << "Launching lacros-chrome at " << chrome_path;

  DCHECK(lacros_selection_.has_value());
  version_info::Channel update_channel =
      browser_util::GetLacrosSelectionUpdateChannel(lacros_selection_.value());
  // If we don't have channel information, we default to the "dev" channel.
  if (update_channel == version_info::Channel::UNKNOWN)
    update_channel = browser_util::kLacrosDefaultChannel;

  base::LaunchOptions options;
  options.environment["EGL_PLATFORM"] = "surfaceless";
  options.environment["XDG_RUNTIME_DIR"] = GetXdgRuntimeDir();
  options.environment["CHROME_VERSION_EXTRA"] =
      version_info::GetChannelString(update_channel);

  std::string additional_env =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLacrosChromeAdditionalEnv);
  base::StringPairs env_pairs;
  if (base::SplitStringIntoKeyValuePairsUsingSubstr(additional_env, '=', "####",
                                                    &env_pairs)) {
    for (const auto& env_pair : env_pairs) {
      if (!env_pair.first.empty()) {
        LOG(WARNING) << "Applying lacros env " << env_pair.first << "="
                     << env_pair.second;
        options.environment[env_pair.first] = env_pair.second;
      }
    }
  }

  options.kill_on_parent_death = true;

  // Paths are UTF-8 safe on Chrome OS.
  std::string user_data_dir = browser_util::GetUserDataDir().AsUTF8Unsafe();
  std::string crash_dir =
      browser_util::GetUserDataDir().Append("crash_dumps").AsUTF8Unsafe();

  // Pass the locale via command line instead of via LacrosInitParams because
  // the Lacros browser process needs it early in startup, before zygote fork.
  std::string locale = g_browser_process->GetApplicationLocale();

  // Static configuration should be enabled from Lacros rather than Ash. This
  // vector should only be used for dynamic configuration.
  // TODO(https://crbug.com/1145713): Remove existing static configuration.
  std::vector<std::string> argv = {chrome_path,
                                   "--ozone-platform=wayland",
                                   "--user-data-dir=" + user_data_dir,
                                   "--enable-gpu-rasterization",
                                   "--enable-oop-rasterization",
                                   "--lang=" + locale,
                                   "--enable-crashpad",
                                   "--enable-webgl-image-chromium",
                                   "--breakpad-dump-location=" + crash_dir};

  // CrAS is the default audio server in Chrome OS.
  if (base::SysInfo::IsRunningOnChromeOS())
    argv.push_back("--use-cras");

  std::string additional_flags =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kLacrosChromeAdditionalArgs);
  std::vector<base::StringPiece> delimited_flags =
      base::SplitStringPieceUsingSubstr(additional_flags, "####",
                                        base::TRIM_WHITESPACE,
                                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& flag : delimited_flags)
    argv.emplace_back(flag);

  // If logfd is valid, enable logging and redirect stdout/stderr to logfd.
  if (params.logfd.is_valid()) {
    // The next flag will make chrome log only via stderr. See
    // DetermineLoggingDestination in logging_chrome.cc.
    argv.push_back("--enable-logging=stderr");

    // These options will assign stdout/stderr fds to logfd in the fd table of
    // the new process.
    options.fds_to_remap.push_back(
        std::make_pair(params.logfd.get(), STDOUT_FILENO));
    options.fds_to_remap.push_back(
        std::make_pair(params.logfd.get(), STDERR_FILENO));
  }

  base::ScopedFD startup_fd = browser_util::CreateStartupData(
      environment_provider_.get(), std::move(initial_browser_action),
      !keep_alive_features_.empty());
  if (startup_fd.is_valid()) {
    // Hardcoded to use FD 3 to make the ash-chrome's behavior more predictable.
    // Lacros-chrome should not depend on the hardcoded value though. Instead
    // it should take a look at the value passed via the command line flag.
    constexpr int kStartupDataFD = 3;
    argv.push_back(base::StringPrintf(
        "--%s=%d", chromeos::switches::kCrosStartupDataFD, kStartupDataFD));
    options.fds_to_remap.emplace_back(startup_fd.get(), kStartupDataFD);
  }

  // Set up Mojo channel.
  base::CommandLine command_line(argv);
  LOG(WARNING) << "Launching lacros with command: "
               << command_line.GetCommandLineString();

  // Lacros-chrome starts with NORMAL priority
  LacrosThreadPriorityDelegate thread_priority_delegate;
  options.pre_exec_delegate = &thread_priority_delegate;

  // Prepare to invite lacros-chrome to the Mojo universe of Crosapi.
  mojo::PlatformChannel channel;
  std::string channel_flag_value;
  channel.PrepareToPassRemoteEndpoint(&options.fds_to_remap,
                                      &channel_flag_value);
  DCHECK(!channel_flag_value.empty());
  command_line.AppendSwitchASCII(kCrosapiMojoPlatformChannelHandle,
                                 channel_flag_value);
  DCHECK(!crosapi_id_.has_value());
  // Use new Crosapi mojo connection to detect process termination always.
  crosapi_id_ = CrosapiManager::Get()->SendInvitation(
      channel.TakeLocalEndpoint(),
      base::BindOnce(&BrowserManager::OnMojoDisconnected,
                     weak_factory_.GetWeakPtr()));

  // Append a fake switch for backward compatibility.
  // TODO(crbug.com/1188020): Remove this after M93 Lacros is spread enough.
  command_line.AppendSwitchASCII(mojo::PlatformChannel::kHandleSwitch, "-1");

  // Create the lacros-chrome subprocess.
  base::RecordAction(base::UserMetricsAction("Lacros.Launch"));
  lacros_launch_time_ = base::TimeTicks::Now();
  // If lacros_process_ already exists, because it does not call waitpid(2),
  // the process will never be collected.
  lacros_process_ = base::LaunchProcess(command_line, options);
  if (!lacros_process_.IsValid()) {
    LOG(ERROR) << "Failed to launch lacros-chrome";
    SetState(State::STOPPED);
    return;
  }
  SetState(State::STARTING);
  LOG(WARNING) << "Launched lacros-chrome with pid " << lacros_process_.Pid();
  channel.RemoteProcessLaunchAttempted();
}

void BrowserManager::OnBrowserServiceConnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id,
    mojom::BrowserService* browser_service,
    uint32_t browser_service_version) {
  if (id != crosapi_id_ && id != legacy_crosapi_id_) {
    // This BrowserService is unrelated to this instance. Skipping.
    return;
  }

  DCHECK(!browser_service_.has_value());
  browser_service_ =
      BrowserServiceInfo{mojo_id, browser_service, browser_service_version};
  base::UmaHistogramMediumTimes("ChromeOS.Lacros.StartTime",
                                base::TimeTicks::Now() - lacros_launch_time_);
  // Set the launch-on-login pref every time lacros-chrome successfully starts,
  // instead of once during ash-chrome shutdown, so we have the right value
  // even if ash-chrome crashes.
  SetLaunchOnLoginPref(true);
  LOG(WARNING) << "Connection to lacros-chrome is established.";

  DCHECK_EQ(state_, State::STARTING);
  SetState(State::RUNNING);

  // There can be a chance that keep_alive status is updated between the
  // process launching timing (where initial_keep_alive is set) and the
  // crosapi mojo connection timing (i.e., this function).
  // So, send it to lacros-chrome to update to fill the possible gap.
  UpdateKeepAliveInBrowserIfNecessary(!keep_alive_features_.empty());
}

void BrowserManager::OnBrowserServiceDisconnected(
    CrosapiId id,
    mojo::RemoteSetElementId mojo_id) {
  // No need to check CrosapiId here, because |mojo_id| is unique within
  // a process.
  if (browser_service_.has_value() && browser_service_->mojo_id == mojo_id)
    browser_service_.reset();
}

void BrowserManager::OnBrowserRelaunchRequested(CrosapiId id) {
  if (id != crosapi_id_)
    return;
  relaunch_requested_ = true;
}

void BrowserManager::OnMojoDisconnected() {
  DCHECK(state_ == State::STARTING || state_ == State::RUNNING);
  DCHECK(lacros_process_.IsValid());
  LOG(WARNING)
      << "Mojo to lacros-chrome is disconnected. Terminating lacros-chrome";

  browser_service_.reset();
  crosapi_id_.reset();
  legacy_crosapi_id_.reset();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&TerminateLacrosChrome, std::move(lacros_process_)),
      base::BindOnce(&BrowserManager::OnLacrosChromeTerminated,
                     weak_factory_.GetWeakPtr()));

  SetState(State::TERMINATING);
}

void BrowserManager::OnLacrosChromeTerminated() {
  DCHECK_EQ(state_, State::TERMINATING);
  LOG(WARNING) << "Lacros-chrome is terminated";
  SetState(State::STOPPED);
  // TODO(https://crbug.com/1109366): Restart lacros-chrome if it exits
  // abnormally (e.g. crashes). For now, assume the user meant to close it.
  SetLaunchOnLoginPref(false);

  if (!shutdown_requested_ && relaunch_requested_) {
    Start(browser_util::InitialBrowserAction(
        mojom::InitialBrowserAction::kRestoreLastSession));
  }
}

void BrowserManager::OnSessionStateChanged() {
  // Wait for session to become active.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->session_state() !=
      session_manager::SessionState::ACTIVE) {
    LOG(WARNING)
        << "Session not yet active. Lacros-chrome will not be launched yet";
    return;
  }

  InitializeAndStart();
}

void BrowserManager::OnStoreLoaded(policy::CloudPolicyStore* store) {
  // A new policy got installed for the current user, so we need to pass it to
  // the Lacros browser.
  std::string policy_blob;
  bool success =
      store->policy_fetch_response()->SerializeToString(&policy_blob);
  DCHECK(success);
  SetDeviceAccountPolicy(policy_blob);
}

void BrowserManager::OnStoreError(policy::CloudPolicyStore* store) {
  // Policy store failed, Lacros will use stale policy as well as Ash.
}

void BrowserManager::OnStoreDestruction(policy::CloudPolicyStore* store) {
  store->RemoveObserver(this);
}

void BrowserManager::OnLoadComplete(const base::FilePath& path,
                                    LacrosSelection selection) {
  DCHECK_EQ(state_, State::MOUNTING);

  lacros_path_ = path;
  lacros_selection_ = absl::optional<LacrosSelection>(selection);
  SetState(path.empty() ? State::UNAVAILABLE : State::STOPPED);
  if (load_complete_callback_) {
    const bool success = !path.empty();
    std::move(load_complete_callback_).Run(success);
  }

  if (state_ == State::STOPPED && !shutdown_requested_ &&
      (browser_util::IsLacrosPrimaryBrowser() || GetLaunchOnLoginPref())) {
    Start(GetInitialBrowserAction());
  }
}

void BrowserManager::PrepareLacrosPolicies() {
  policy::CloudPolicyStore* store = GetDeviceAccountPolicyStore();
  if (!store)
    return;

  if (!store->policy_fetch_response())
    return;
  const std::string policy_blob =
      store->policy_fetch_response()->SerializeAsString();
  SetDeviceAccountPolicy(policy_blob);
  // The lifetime of `BrowserManager` is longer than lifetime of policy store.
  // That is why `CloudPolicyStore::RemoveObserver()` is called during
  // `CloudPolicyStore::Observer::OnStoreDestruction()`.
  store->AddObserver(this);
}

policy::CloudPolicyStore* BrowserManager::GetDeviceAccountPolicyStore() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(user);

  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR: {
      Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
      DCHECK(profile);
      policy::CloudPolicyManager* user_cloud_policy_manager =
          profile->GetUserCloudPolicyManagerAsh();
      if (!user_cloud_policy_manager)
        return nullptr;
      return user_cloud_policy_manager->core()->store();
    }
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_WEB_KIOSK_APP: {
      policy::DeviceLocalAccountPolicyBroker* broker =
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->GetDeviceLocalAccountPolicyService()
              ->GetBrokerForUser(user->GetAccountId().GetUserEmail());
      return broker ? broker->core()->store() : nullptr;
    }
    default:
      return nullptr;
  }
}

void BrowserManager::SetDeviceAccountPolicy(const std::string& policy_blob) {
  environment_provider_->SetDeviceAccountPolicy(policy_blob);
  if (browser_service_.has_value()) {
    browser_service_->service->UpdateDeviceAccountPolicy(
        std::vector<uint8_t>(policy_blob.begin(), policy_blob.end()));
  }
}

LaunchParamsFromBackground::LaunchParamsFromBackground() = default;
LaunchParamsFromBackground::LaunchParamsFromBackground(
    LaunchParamsFromBackground&&) = default;
LaunchParamsFromBackground::~LaunchParamsFromBackground() = default;

void BrowserManager::StartKeepAlive(Feature feature) {
  DCHECK(keep_alive_features_.find(feature) == keep_alive_features_.end())
      << "Features should never be double registered.";

  keep_alive_features_.insert(feature);
  // If this is first KeepAlive instance, update the keep-alive in the browser.
  if (keep_alive_features_.size() == 1) {
    // If browser is not running, we have to launch it.
    LaunchForKeepAliveIfNecessary();
    UpdateKeepAliveInBrowserIfNecessary(true);
  }
}

void BrowserManager::StopKeepAlive(Feature feature) {
  keep_alive_features_.erase(feature);
  if (keep_alive_features_.empty())
    UpdateKeepAliveInBrowserIfNecessary(false);
}

void BrowserManager::LaunchForKeepAliveIfNecessary() {
  if (state_ == State::STOPPED && !shutdown_requested_ &&
      !keep_alive_features_.empty()) {
    CHECK(browser_util::IsLacrosEnabled());
    CHECK(browser_util::IsLacrosAllowedToLaunch());
    Start(browser_util::InitialBrowserAction(
        mojom::InitialBrowserAction::kDoNotOpenWindow));
  }
}

void BrowserManager::UpdateKeepAliveInBrowserIfNecessary(bool enabled) {
  if (shutdown_requested_ || !browser_service_.has_value() ||
      browser_service_->interface_version <
          crosapi::mojom::BrowserService::kUpdateKeepAliveMinVersion) {
    // Shutdown has started, the browser is not running now, or Lacros is too
    // old. Just give up.
    return;
  }
  browser_service_->service->UpdateKeepAlive(enabled);
}

void BrowserManager::RecordLacrosLaunchMode() {
  LacrosLaunchMode lacros_mode;
  if (!browser_util::IsAshWebBrowserEnabled(chrome::GetChannel())) {
    // As Ash is disabled, Lacros is the only available browser.
    lacros_mode = LacrosLaunchMode::kLacrosOnly;
  } else if (browser_util::IsLacrosPrimaryBrowser()) {
    // Lacros is the primary browser - but Ash is still available.
    lacros_mode = LacrosLaunchMode::kLacrosPrimary;
  } else if (browser_util::IsLacrosEnabled()) {
    // If Lacros is enabled but not primary or the only browser, the
    // side by side mode is active.
    lacros_mode = LacrosLaunchMode::kSideBySide;
  } else {
    lacros_mode = LacrosLaunchMode::kLacrosDisabled;
  }

  UMA_HISTOGRAM_ENUMERATION("Ash.Lacros.Launch.Mode", lacros_mode);
}

}  // namespace crosapi
