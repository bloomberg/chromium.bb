// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_util.h"

#include <linux/magic.h>
#include <sys/statfs.h>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/configuration_keys.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/oobe_configuration.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace arc {

namespace {

// Contains map of profile to check result of ARC allowed. Contains true if ARC
// allowed check was performed and ARC is allowed. If map does not contain
// a value then this means that check has not been performed yet.
base::LazyInstance<std::map<const Profile*, bool>>::DestructorAtExit
    g_profile_status_check = LAZY_INSTANCE_INITIALIZER;

// The cached value of migration allowed for profile. It is necessary to use
// the same value during a user session.
base::LazyInstance<std::map<base::FilePath, bool>>::DestructorAtExit
    g_is_arc_migration_allowed = LAZY_INSTANCE_INITIALIZER;

// Let IsAllowedForProfile() return "false" for any profile.
bool g_disallow_for_testing = false;

// Let IsArcBlockedDueToIncompatibleFileSystem() return the specified value
// during test runs. Doesn't affect ARC kiosk and public session.
bool g_arc_blocked_due_to_incompatible_filesystem_for_testing = false;

// TODO(kinaba): Temporary workaround for crbug.com/729034.
//
// Some type of accounts don't have user prefs. As a short-term workaround,
// store the compatibility info from them on memory, ignoring the defect that
// it cannot survive browser crash and restart.
//
// This will be removed once the forced migration for ARC Kiosk user is
// implemented. After it's done such types of accounts cannot even sign-in
// with incompatible filesystem. Hence it'll be safe to always regard compatible
// for them then.
base::LazyInstance<std::set<AccountId>>::DestructorAtExit
    g_known_compatible_users = LAZY_INSTANCE_INITIALIZER;

// Returns whether ARC can run on the filesystem mounted at |path|.
// This function should run only on threads where IO operations are allowed.
bool IsArcCompatibleFilesystem(const base::FilePath& path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // If it can be verified it is not on ecryptfs, then it is ok.
  struct statfs statfs_buf;
  if (statfs(path.value().c_str(), &statfs_buf) < 0)
    return false;
  return statfs_buf.f_type != ECRYPTFS_SUPER_MAGIC;
}

FileSystemCompatibilityState GetFileSystemCompatibilityPref(
    const AccountId& account_id) {
  int pref_value = kFileSystemIncompatible;
  user_manager::known_user::GetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen, &pref_value);
  return static_cast<FileSystemCompatibilityState>(pref_value);
}

// Stores the result of IsArcCompatibleFilesystem posted back from the blocking
// task runner.
void StoreCompatibilityCheckResult(const AccountId& account_id,
                                   base::OnceClosure callback,
                                   bool is_compatible) {
  if (is_compatible) {
    user_manager::known_user::SetIntegerPref(
        account_id, prefs::kArcCompatibleFilesystemChosen,
        kFileSystemCompatible);

    // TODO(kinaba): Remove this code for accounts without user prefs.
    // See the comment for |g_known_compatible_users| for the detail.
    if (GetFileSystemCompatibilityPref(account_id) != kFileSystemCompatible)
      g_known_compatible_users.Get().insert(account_id);
  }
  std::move(callback).Run();
}

bool IsArcMigrationAllowedInternal(const Profile* profile) {
  return static_cast<policy_util::EcryptfsMigrationAction>(
             profile->GetPrefs()->GetInteger(
                 prefs::kEcryptfsMigrationStrategy)) !=
         policy_util::EcryptfsMigrationAction::kDisallowMigration;
}

bool IsUnaffiliatedArcAllowed() {
  bool arc_allowed;
  ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
  if (arc_session_manager) {
    switch (arc_session_manager->state()) {
      case ArcSessionManager::State::NOT_INITIALIZED:
      case ArcSessionManager::State::STOPPED:
        // Apply logic below
        break;
      case ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE:
      case ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT:
      case ArcSessionManager::State::REMOVING_DATA_DIR:
      case ArcSessionManager::State::ACTIVE:
      case ArcSessionManager::State::STOPPING:
        // Never forbid unaffiliated ARC while ARC is running
        return true;
    }
  }
  if (chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kUnaffiliatedArcAllowed, &arc_allowed)) {
    return arc_allowed;
  }
  // If device policy is not set, allow ARC.
  return true;
}

bool IsArcAllowedForProfileInternal(const Profile* profile,
                                    bool should_report_reason) {
  if (g_disallow_for_testing) {
    VLOG_IF(1, should_report_reason) << "ARC is disallowed for testing.";
    return false;
  }

  // ARC Kiosk can be enabled even if ARC is not yet supported on the device.
  // In that case IsArcKioskMode() should return true as profile is already
  // created.
  if (!IsArcAvailable() && !(IsArcKioskMode() && IsArcKioskAvailable())) {
    VLOG_IF(1, should_report_reason) << "ARC is not available.";
    return false;
  }

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    VLOG_IF(1, should_report_reason)
        << "Non-primary users are not supported in ARC.";
    return false;
  }

  if (profile->IsLegacySupervised()) {
    VLOG_IF(1, should_report_reason)
        << "Supervised users are not supported in ARC.";
    return false;
  }

  if (IsArcBlockedDueToIncompatibleFileSystem(profile) &&
      !IsArcMigrationAllowedByPolicyForProfile(profile)) {
    VLOG_IF(1, should_report_reason)
        << "Incompatible encryption and migration forbidden.";
    return false;
  }

  if (policy_util::IsArcDisabledForEnterprise() &&
      policy_util::IsAccountManaged(profile)) {
    VLOG_IF(1, should_report_reason)
        << "ARC is disabled by flag for managed users.";
    return false;
  }

  // Play Store requires an appropriate application install mechanism. Normal
  // users do this through GAIA, but Kiosk and Active Directory users use
  // different application install mechanism. ARC is not allowed otherwise
  // (e.g. in public sessions). cf) crbug.com/605545
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsArcAllowedForUser(user)) {
    VLOG_IF(1, should_report_reason) << "ARC is not allowed for the user.";
    return false;
  }

  if (!user->IsAffiliated() && !IsUnaffiliatedArcAllowed()) {
    VLOG_IF(1, should_report_reason)
        << "Device admin disallowed ARC for unaffiliated users.";
    return false;
  }

  // Do not run ARC instance when supervised user is being created.
  // Otherwise noisy notification may be displayed.
  chromeos::UserFlow* user_flow =
      chromeos::ChromeUserManager::Get()->GetUserFlow(user->GetAccountId());
  if (!user_flow || !user_flow->CanStartArc()) {
    VLOG_IF(1, should_report_reason)
        << "ARC is not allowed in the current user flow.";
    return false;
  }

  return true;
}

void ShowContactAdminDialog() {
  chrome::ShowWarningMessageBox(
      nullptr, l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_CONTACT_ADMIN_TITLE),
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_CONTACT_ADMIN_CONTEXT));
}

}  // namespace

bool IsRealUserProfile(const Profile* profile) {
  // Return false for signin, lock screen and incognito profiles.
  return profile && !chromeos::ProfileHelper::IsSigninProfile(profile) &&
         !chromeos::ProfileHelper::IsLockScreenAppProfile(profile) &&
         !profile->IsOffTheRecord();
}

bool IsArcAllowedForProfile(const Profile* profile) {
  if (!IsRealUserProfile(profile))
    return false;

  auto it = g_profile_status_check.Get().find(profile);

  const bool first_check = it == g_profile_status_check.Get().end();
  const bool result =
      IsArcAllowedForProfileInternal(profile, first_check /* report_reason */);

  if (first_check) {
    g_profile_status_check.Get()[profile] = result;
    return result;
  }

  // This is next check. We should be persistent and report the same result.
  if (result != it->second) {
    NOTREACHED() << "ARC allowed was changed for the current user session "
                 << "and profile " << profile->GetPath().MaybeAsASCII()
                 << ". This may lead to unexpected behavior. ARC allowed is"
                 << " forced to " << it->second;
  }
  return it->second;
}

bool IsArcProvisioned(const Profile* profile) {
  return profile && profile->GetPrefs()->HasPrefPath(prefs::kArcSignedIn) &&
         profile->GetPrefs()->GetBoolean(prefs::kArcSignedIn);
}

void ResetArcAllowedCheckForTesting(const Profile* profile) {
  g_profile_status_check.Get().erase(profile);
}

bool IsArcMigrationAllowedByPolicyForProfile(const Profile* profile) {
  // Always allow migration for unmanaged users.
  if (!profile || !policy_util::IsAccountManaged(profile))
    return true;

  // Use the profile path as unique identifier for profile.
  const base::FilePath path = profile->GetPath();
  auto iter = g_is_arc_migration_allowed.Get().find(path);
  if (iter == g_is_arc_migration_allowed.Get().end()) {
    iter = g_is_arc_migration_allowed.Get()
               .emplace(path, IsArcMigrationAllowedInternal(profile))
               .first;
  }

  return iter->second;
}

bool IsArcBlockedDueToIncompatibleFileSystem(const Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);

  // Return true for public accounts as they only have ext4 and
  // for ARC kiosk as migration to ext4 should always be triggered.
  // Without this check it fails to start after browser crash as
  // compatibility info is stored in RAM.
  if (user && (user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
               user->GetType() == user_manager::USER_TYPE_ARC_KIOSK_APP)) {
    return false;
  }

  // Test runs on Linux workstation does not have expected /etc/lsb-release
  // field nor profile creation step. Hence it returns a dummy test value.
  if (!base::SysInfo::IsRunningOnChromeOS())
    return g_arc_blocked_due_to_incompatible_filesystem_for_testing;

  // Conducts the actual check, only when running on a real Chrome OS device.
  return !IsArcCompatibleFileSystemUsedForUser(user);
}

void SetArcBlockedDueToIncompatibleFileSystemForTesting(bool block) {
  g_arc_blocked_due_to_incompatible_filesystem_for_testing = block;
}

bool IsArcCompatibleFileSystemUsedForUser(const user_manager::User* user) {
  // Returns false for profiles not associated with users (like sign-in profile)
  if (!user)
    return false;

  // chromeos::UserSessionManager does the actual file system check and stores
  // the result to prefs, so that it survives crash-restart.
  FileSystemCompatibilityState filesystem_compatibility =
      GetFileSystemCompatibilityPref(user->GetAccountId());
  const bool is_filesystem_compatible =
      filesystem_compatibility != kFileSystemIncompatible ||
      g_known_compatible_users.Get().count(user->GetAccountId()) != 0;

  // To run ARC we want to make sure the underlying file system is compatible
  // with ARC.
  if (!is_filesystem_compatible) {
    VLOG(1)
        << "ARC is not supported since the user hasn't migrated to dircrypto.";
    return false;
  }

  return true;
}

void DisallowArcForTesting() {
  g_disallow_for_testing = true;
  g_profile_status_check.Get().clear();
}

bool IsArcPlayStoreEnabledForProfile(const Profile* profile) {
  return IsArcAllowedForProfile(profile) &&
         profile->GetPrefs()->GetBoolean(prefs::kArcEnabled);
}

bool IsArcPlayStoreEnabledPreferenceManagedForProfile(const Profile* profile) {
  if (!IsArcAllowedForProfile(profile)) {
    LOG(DFATAL) << "ARC is not allowed for profile";
    return false;
  }
  return profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled);
}

bool SetArcPlayStoreEnabledForProfile(Profile* profile, bool enabled) {
  DCHECK(IsArcAllowedForProfile(profile));
  if (IsArcPlayStoreEnabledPreferenceManagedForProfile(profile)) {
    if (enabled && !IsArcPlayStoreEnabledForProfile(profile)) {
      LOG(WARNING) << "Attempt to enable disabled by policy ARC.";
      if (chromeos::switches::IsTabletFormFactor()) {
        VLOG(1) << "Showing contact admin dialog managed user of tablet form "
                   "factor devices.";
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&ShowContactAdminDialog));
      }
      return false;
    }
    VLOG(1) << "Google-Play-Store-enabled pref is managed. Request to "
            << (enabled ? "enable" : "disable") << " Play Store is not stored";
    // Need update ARC session manager manually for managed case in order to
    // keep its state up to date, otherwise it may stuck with enabling
    // request.
    // TODO(khmel): Consider finding the better way handling this.
    ArcSessionManager* arc_session_manager = ArcSessionManager::Get();
    // |arc_session_manager| can be nullptr in unit_tests.
    if (!arc_session_manager)
      return false;
    if (enabled) {
      arc_session_manager->RequestEnable();
    } else {
      // Before calling RequestDisable here we cache enable_requested because
      // RequestArcDataRemoval was refactored outside of RequestDisable where
      // it was called only in case enable_requested was true (RequestDisable
      // sets enable_requested to false).
      const bool enable_requested = arc_session_manager->enable_requested();
      arc_session_manager->RequestDisable();
      if (enable_requested)
        arc_session_manager->RequestArcDataRemoval();
    }
    return true;
  }
  profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, enabled);
  return true;
}

bool AreArcAllOptInPreferencesIgnorableForProfile(const Profile* profile) {
  // For Active Directory users, a LaForge account is created, where
  // backup&restore and location services are not supported, hence the policies
  // are unused.
  if (IsActiveDirectoryUserForProfile(profile))
    return true;

  // Otherwise, the preferences are ignorable iff both backup&restore and
  // location services are set by policy.
  const PrefService* prefs = profile->GetPrefs();
  return prefs->IsManagedPreference(prefs::kArcBackupRestoreEnabled) &&
         prefs->IsManagedPreference(prefs::kArcLocationServiceEnabled);
}

bool IsActiveDirectoryUserForProfile(const Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user ? user->IsActiveDirectoryUser() : false;
}

bool IsArcOobeOptInActive() {
  // No OOBE is expected in case Play Store is not available.
  if (!IsPlayStoreAvailable())
    return false;

  // Check if Chrome OS OOBE flow is currently showing.
  // TODO(b/65861628): Redesign the OptIn flow since there is no longer reason
  // to have two different OptIn flows.
  if (!chromeos::LoginDisplayHost::default_host())
    return false;

  // Use the legacy logic for first sign-in OOBE OptIn flow. Make sure the user
  // is new.
  return user_manager::UserManager::Get()->IsCurrentUserNew();
}

bool IsArcOobeOptInConfigurationBased() {
  // Ignore if not applicable.
  if (!IsArcOobeOptInActive())
    return false;
  // Check that configuration exist.
  auto* oobe_configuration = chromeos::OobeConfiguration::Get();
  if (!oobe_configuration)
    return false;
  if (!oobe_configuration->CheckCompleted())
    return false;
  // Check configuration value that triggers automatic ARC TOS acceptance.
  auto& configuration = oobe_configuration->GetConfiguration();
  auto* auto_accept = configuration.FindKeyOfType(
      chromeos::configuration::kArcTosAutoAccept, base::Value::Type::BOOLEAN);
  if (!auto_accept)
    return false;
  return auto_accept->GetBool();
}

bool IsArcTermsOfServiceNegotiationNeeded(const Profile* profile) {
  DCHECK(profile);
  // Don't show in session ARC OptIn dialog for managed user.
  // For more info see crbug/950013.
  // Skip to show UI asking users to set up ARC OptIn preferences, if all of
  // them are managed by the admin policy. Note that the ToS agreement is anyway
  // not shown in the case of the managed ARC.
  if (ShouldStartArcSilentlyForManagedProfile(profile) &&
      !ShouldShowOptInForTesting()) {
    VLOG(1) << "Skip ARC Terms of Service negotiation for managed user. "
            << "Don't record B&R and GLS if admin leave it as user to decide "
            << "and user sikps the opt-in dialog.";
    return false;
  }

  // If it is marked that the Terms of service is accepted already,
  // just skip the negotiation with user, and start Android management
  // check directly.
  // This happens, e.g., when a user accepted the Terms of service on Opt-in
  // flow, but logged out before ARC sign in procedure was done. Then, logs
  // in again.
  if (profile->GetPrefs()->GetBoolean(prefs::kArcTermsAccepted)) {
    VLOG(1) << "The user already accepts ARC Terms of Service.";
    return false;
  }

  return true;
}

bool IsArcTermsOfServiceOobeNegotiationNeeded() {
  if (!IsPlayStoreAvailable()) {
    VLOG(1) << "Skip ARC Terms of Service screen because Play Store is not "
               "available on the device.";
    return false;
  }

  // Demo mode setup flow runs before user is created, therefore this condition
  // needs to be checked before any user related ones.
  if (IsArcDemoModeSetupFlow())
    return true;

  if (!user_manager::UserManager::Get()->IsUserLoggedIn()) {
    VLOG(1) << "Skip ARC Terms of Service screen because user is not "
            << "logged in.";
    return false;
  }

  const Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!IsArcAllowedForProfile(profile)) {
    VLOG(1) << "Skip ARC Terms of Service screen because ARC is not allowed.";
    return false;
  }

  if (profile->GetPrefs()->IsManagedPreference(prefs::kArcEnabled) &&
      !profile->GetPrefs()->GetBoolean(prefs::kArcEnabled)) {
    VLOG(1) << "Skip ARC Terms of Service screen because ARC is disabled.";
    return false;
  }

  if (IsActiveDirectoryUserForProfile(profile)) {
    VLOG(1) << "Skip ARC Terms of Service screen because it does not apply to "
               "Active Directory users.";
    return false;
  }

  if (!IsArcTermsOfServiceNegotiationNeeded(profile)) {
    VLOG(1) << "Skip ARC Terms of Service screen because it is already "
               "accepted or fully controlled by policy.";
    return false;
  }

  return true;
}

bool IsArcStatsReportingEnabled() {
  // Public session users never saw the consent for stats reporting even if the
  // admin forced the pref by a policy.
  if (profiles::IsPublicSession()) {
    return false;
  }

  bool pref = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &pref);
  return pref;
}

bool IsArcDemoModeSetupFlow() {
  return chromeos::DemoSetupController::IsOobeDemoSetupFlowInProgress();
}

void UpdateArcFileSystemCompatibilityPrefIfNeeded(
    const AccountId& account_id,
    const base::FilePath& profile_path,
    base::OnceClosure callback) {
  DCHECK(callback);

  // If ARC is not available, skip the check.
  // This shortcut is just for merginally improving the log-in performance on
  // old devices without ARC. We can always safely remove the following 4 lines
  // without changing any functionality when, say, the code clarity becomes
  // more important in the future.
  if (!IsArcAvailable() && !IsArcKioskAvailable()) {
    std::move(callback).Run();
    return;
  }

  // If the compatibility has been already confirmed, skip the check.
  if (GetFileSystemCompatibilityPref(account_id) != kFileSystemIncompatible) {
    std::move(callback).Run();
    return;
  }

  // Otherwise, check the underlying filesystem.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&IsArcCompatibleFilesystem, profile_path),
      base::BindOnce(&StoreCompatibilityCheckResult, account_id,
                     std::move(callback)));
}

ArcSupervisionTransition GetSupervisionTransition(const Profile* profile) {
  DCHECK(profile);
  DCHECK(profile->GetPrefs());

  const ArcSupervisionTransition supervision_transition =
      static_cast<ArcSupervisionTransition>(
          profile->GetPrefs()->GetInteger(prefs::kArcSupervisionTransition));
  const bool is_child_to_regular_enabled =
      base::FeatureList::IsEnabled(kEnableChildToRegularTransitionFeature);
  const bool is_regular_to_child_enabled =
      base::FeatureList::IsEnabled(kEnableRegularToChildTransitionFeature);

  switch (supervision_transition) {
    case ArcSupervisionTransition::NO_TRANSITION:
      // Do nothing.
      break;
    case ArcSupervisionTransition::CHILD_TO_REGULAR:
      if (!is_child_to_regular_enabled)
        return ArcSupervisionTransition::NO_TRANSITION;
      break;
    case ArcSupervisionTransition::REGULAR_TO_CHILD:
      if (!is_regular_to_child_enabled)
        return ArcSupervisionTransition::NO_TRANSITION;
      break;
  }
  return supervision_transition;
}

bool IsPlayStoreAvailable() {
  if (ShouldArcAlwaysStartWithNoPlayStore())
    return false;

  if (!IsRobotOrOfflineDemoAccountMode())
    return true;

  // Demo Mode is the only public session scenario that can launch Play.
  return chromeos::DemoSession::IsDeviceInDemoMode() &&
         chromeos::features::ShouldShowPlayStoreInDemoMode();
}

bool ShouldStartArcSilentlyForManagedProfile(const Profile* profile) {
  return IsArcPlayStoreEnabledPreferenceManagedForProfile(profile) &&
         (AreArcAllOptInPreferencesIgnorableForProfile(profile) ||
          !IsArcOobeOptInActive());
}

aura::Window* GetArcWindow(int32_t task_id) {
  for (auto* window : ChromeLauncherController::instance()->GetArcWindows()) {
    if (arc::GetWindowTaskId(window) == task_id)
      return window;
  }

  return nullptr;
}

std::unique_ptr<content::WebContents> CreateArcCustomTabWebContents(
    Profile* profile,
    const GURL& url) {
  scoped_refptr<content::SiteInstance> site_instance =
      tab_util::GetSiteInstanceForNewTab(profile, url);
  content::WebContents::CreateParams create_params(profile, site_instance);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  // Use the same version number as browser_commands.cc
  // TODO(hashimoto): Get the actual Android version from the container.
  constexpr char kOsOverrideForTabletSite[] = "Linux; Android 9; Chrome tablet";
  // Override the user agent to request mobile version web sites.
  const std::string product =
      version_info::GetProductNameAndVersionForUserAgent();
  const std::string user_agent = content::BuildUserAgentFromOSAndProduct(
      kOsOverrideForTabletSite, product);
  web_contents->SetUserAgentOverride(user_agent,
                                     false /*override_in_new_tabs=*/);

  content::NavigationController::LoadURLParams load_url_params(url);
  load_url_params.source_site_instance = site_instance;
  load_url_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_TRUE;
  web_contents->GetController().LoadURLWithParams(load_url_params);

  // Add a flag to remember this tab originated in the ARC context.
  web_contents->SetUserData(&arc::ArcWebContentsData::kArcTransitionFlag,
                            std::make_unique<arc::ArcWebContentsData>());

  return web_contents;
}

std::string GetHistogramNameByUserType(const std::string& base_name,
                                       const Profile* profile) {
  if (profile == nullptr) {
    profile = ProfileManager::GetPrimaryUserProfile();
  }
  if (IsRobotOrOfflineDemoAccountMode()) {
    chromeos::DemoSession* demo_session = chromeos::DemoSession::Get();
    if (demo_session && demo_session->started()) {
      return demo_session->offline_enrolled() ? base_name + ".OfflineDemoMode"
                                              : base_name + ".DemoMode";
    }
    return base_name + ".RobotAccount";
  }
  if (profile->IsChild())
    return base_name + ".Child";
  if (IsActiveDirectoryUserForProfile(profile))
    return base_name + ".ActiveDirectory";
  return base_name +
         (policy_util::IsAccountManaged(profile) ? ".Managed" : ".Unmanaged");
}

std::string GetHistogramNameByUserTypeForPrimaryProfile(
    const std::string& base_name) {
  return GetHistogramNameByUserType(base_name, /*profile=*/nullptr);
}

}  // namespace arc
