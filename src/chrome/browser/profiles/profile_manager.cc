// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/ash/account_manager/child_account_type_changed_user_data.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_lifetime_manager_factory.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lite_video/lite_video_keyed_service.h"
#include "chrome/browser/lite_video/lite_video_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/adaptive_quiet_notification_permission_ui_enabler.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/default_search_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/accessibility/live_caption_controller_factory.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/live_caption/live_caption_controller.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller_factory.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_management_transition.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/profiles/profile_statistics.h"
#include "chrome/browser/profiles/profile_statistics_factory.h"
#endif

#if defined(OS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/signin_util_win.h"
#endif  // defined(OS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

// Used in metrics for NukeProfileFromDisk(). Keep in sync with enums.xml.
//
// Entries should not be renumbered and numeric values should never be reused.
//
// Note: there are maximum 3 attempts to nuke a profile.
enum class NukeProfileResult {
  // Success values. Make sure they are consecutive.
  kSuccessFirstAttempt = 0,
  kSuccessSecondAttempt = 1,
  kSuccessThirdAttempt = 2,

  // Failure values. Make sure they are consecutive.
  kFailureFirstAttempt = 10,
  kFailureSecondAttempt = 11,
  kFailureThirdAttempt = 12,
  kMaxValue = kFailureThirdAttempt,
};

const size_t kNukeProfileMaxRetryCount = 3;

// Profile deletion can pass through two stages:
enum class ProfileDeletionStage {
  // At SCHEDULING stage some actions are made before profile deletion,
  // where one of them is the closure of browser windows. Deletion is cancelled
  // if the user choose explicitly not to close any of the tabs.
  SCHEDULING,
  // At MARKED stage profile can be safely removed from disk.
  MARKED
};
using ProfileDeletionMap = std::map<base::FilePath, ProfileDeletionStage>;
ProfileDeletionMap& ProfilesToDelete() {
  static base::NoDestructor<ProfileDeletionMap> profiles_to_delete;
  return *profiles_to_delete;
}

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty())
    running_size += iter.GetInfo().GetSize();
  return running_size;
}

// Simple task to log the size of the current profile.
void ProfileSizeTask(const base::FilePath& path, int enabled_app_count) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);

  // Count number of enabled apps in this profile, if we know.
  if (enabled_app_count != -1)
    UMA_HISTOGRAM_COUNTS_10000("Profile.AppCount", enabled_app_count);
}

#if !defined(OS_ANDROID)
// Schedule a profile for deletion if it isn't already scheduled.
// Returns whether the profile has been newly scheduled.
bool ScheduleProfileDirectoryForDeletion(const base::FilePath& path) {
  if (base::Contains(ProfilesToDelete(), path))
    return false;
  ProfilesToDelete()[path] = ProfileDeletionStage::SCHEDULING;
  return true;
}

void MarkProfileDirectoryForDeletion(const base::FilePath& path) {
  DCHECK(!base::Contains(ProfilesToDelete(), path) ||
         ProfilesToDelete()[path] == ProfileDeletionStage::SCHEDULING);
  ProfilesToDelete()[path] = ProfileDeletionStage::MARKED;
  // Remember that this profile was deleted and files should have been deleted
  // on shutdown. In case of a crash remaining files are removed on next start.
  ListPrefUpdate deleted_profiles(g_browser_process->local_state(),
                                  prefs::kProfilesDeleted);
  deleted_profiles->Append(base::FilePathToValue(path));
}

// Cancel a scheduling deletion, so ScheduleProfileDirectoryForDeletion can be
// called again successfully.
void CancelProfileDeletion(const base::FilePath& path) {
  DCHECK(!base::Contains(ProfilesToDelete(), path) ||
         ProfilesToDelete()[path] == ProfileDeletionStage::SCHEDULING);
  ProfilesToDelete().erase(path);
  ProfileMetrics::LogProfileDeleteUser(ProfileMetrics::DELETE_PROFILE_ABORTED);
}
#endif

NukeProfileResult GetNukeProfileResult(size_t retry_count, bool success) {
  DCHECK_LT(retry_count, kNukeProfileMaxRetryCount);
  const size_t value =
      retry_count +
      static_cast<size_t>(success ? NukeProfileResult::kSuccessFirstAttempt
                                  : NukeProfileResult::kFailureFirstAttempt);
  DCHECK_LE(value, static_cast<size_t>(NukeProfileResult::kMaxValue));
  return static_cast<NukeProfileResult>(value);
}

// Implementation of NukeProfileFromDisk(), retrying at most |max_retry_count|
// times on failure. |retry_count| (initially 0) keeps track of the
// number of attempts so far.
void NukeProfileFromDiskImpl(const base::FilePath& profile_path,
                             size_t retry_count,
                             size_t max_retry_count,
                             base::OnceClosure done_callback) {
  // TODO(crbug.com/1191455): Make FileSystemProxy/FileSystemImpl expose its
  // LockTable, and/or fire events when locks are released. That way we could
  // wait for all the locks in |profile_path| to be released, rather than having
  // this retry logic.
  const base::TimeDelta kRetryDelay = base::Seconds(1);

  // Delete both the profile directory and its corresponding cache.
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile_path, &cache_path);

  bool success = base::DeletePathRecursively(profile_path);
  success = base::DeletePathRecursively(cache_path) && success;

  base::UmaHistogramEnumeration("Profile.NukeFromDisk.Result",
                                GetNukeProfileResult(retry_count, success));

  if (!success && retry_count < max_retry_count - 1) {
    // Failed, try again in |kRetryDelay| seconds.
    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDiskImpl, profile_path, retry_count + 1,
                       max_retry_count, std::move(done_callback)),
        kRetryDelay);
    return;
  }

  if (done_callback) {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  }
}

// Physically remove deleted profile directories from disk. Afterwards, calls
// |done_callback| on the UI thread.
void NukeProfileFromDisk(const base::FilePath& profile_path,
                         base::OnceClosure done_callback) {
  NukeProfileFromDiskImpl(profile_path, /*retry_count=*/0,
                          kNukeProfileMaxRetryCount, std::move(done_callback));
}

// Called after a deleted profile was checked and cleaned up.
void ProfileCleanedUp(base::Value profile_path_value) {
  ListPrefUpdate deleted_profiles(g_browser_process->local_state(),
                                  prefs::kProfilesDeleted);
  deleted_profiles->EraseListValue(profile_path_value);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

// Returns the number of installed (and enabled) apps, excluding any component
// apps.
size_t GetEnabledAppCount(Profile* profile) {
  size_t installed_apps = 0u;
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    if ((*iter)->is_app() &&
        (*iter)->location() !=
            extensions::mojom::ManifestLocation::kComponent) {
      ++installed_apps;
    }
  }
  return installed_apps;
}

#endif  // ENABLE_EXTENSIONS

// Once a profile is loaded through LoadProfile this method is executed.
// It will then run |client_callback| with the right profile or null if it was
// unable to load it.
// It might get called more than once with different values of
// |status| but only once the profile is fully initialized will
// |client_callback| be run.
void OnProfileLoaded(ProfileManager::ProfileLoadedCallback& client_callback,
                     bool incognito,
                     Profile* profile,
                     Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_CREATED) {
    // This is an intermediate state where the profile has been created, but is
    // not yet initialized. Ignore this and wait for the next state change.
    return;
  }
  if (status != Profile::CREATE_STATUS_INITIALIZED) {
    LOG(WARNING) << "Profile not loaded correctly";
    std::move(client_callback).Run(nullptr);
    return;
  }
  DCHECK(profile);
  std::move(client_callback)
      .Run(incognito ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     : profile);
}

#if !defined(OS_ANDROID)
// Helper function for ScheduleForcedEphemeralProfileForDeletion.
bool IsRegisteredAsEphemeral(ProfileAttributesStorage* storage,
                             const base::FilePath& profile_dir) {
  ProfileAttributesEntry* entry =
      storage->GetProfileAttributesWithPath(profile_dir);
  return entry && entry->IsEphemeral();
}
#endif

// Helper function that deletes entries from the kProfilesLastActive pref list.
// It is called when every ephemeral profile is handled.
void RemoveFromLastActiveProfilesPrefList(base::FilePath path) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::ListValue* profile_list = update.Get();
  base::Value entry_value = base::Value(path.BaseName().AsUTF8Unsafe());
  profile_list->EraseListValue(entry_value);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsLoggedIn() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}
#endif

bool IsEphemeral(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kForceEphemeralProfiles);
}

int GetTotalRefCount(const std::map<ProfileKeepAliveOrigin, int>& keep_alives) {
  return std::accumulate(
      keep_alives.begin(), keep_alives.end(), 0,
      [](int acc, const auto& pair) { return acc + pair.second; });
}

// Outputs the state of ProfileInfo::keep_alives, for easier debugging. e.g.,
// a Profile with 3 regular windows open, and one Incognito window open would
// write this string:
//    [kBrowserWindow (3), kOffTheRecordProfile (1)]
std::ostream& operator<<(
    std::ostream& out,
    const std::map<ProfileKeepAliveOrigin, int>& keep_alives) {
  out << "[";
  bool first = true;
  for (const auto& pair : keep_alives) {
    if (pair.second == 0)
      continue;
    if (!first)
      out << ", ";
    out << pair.first << " (" << pair.second << ")";
    first = false;
  }
  out << "]";
  return out;
}

base::FilePath GetLastUsedProfileBaseName() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  base::FilePath last_used_profile_base_name =
      local_state->GetFilePath(prefs::kProfileLastUsed);
  // Make sure the system profile can't be the one marked as the last one used
  // since it shouldn't get a browser.
  if (!last_used_profile_base_name.empty() &&
      last_used_profile_base_name.value() != chrome::kSystemProfileDir) {
    return last_used_profile_base_name;
  }

  return base::FilePath::FromASCII(chrome::kInitialProfile);
}

}  // namespace

ProfileManager::ProfileManager(const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir) {
#if !defined(OS_ANDROID)
  closing_all_browsers_subscription_ = chrome::AddClosingAllBrowsersCallback(
      base::BindRepeating(&ProfileManager::OnClosingAllBrowsersChanged,
                          base::Unretained(this)));
#endif

  if (ProfileShortcutManager::IsFeatureEnabled() && !user_data_dir_.empty())
    profile_shortcut_manager_ = ProfileShortcutManager::Create(this);
}

ProfileManager::~ProfileManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_) {
    observer.OnProfileManagerDestroying();
  }
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // Ideally, all the keepalives should've been cleared already. Report
    // metrics for incorrect usage of ScopedProfileKeepAlive.
    for (const auto& path_and_profile_info : profiles_info_) {
      const ProfileInfo* profile_info = path_and_profile_info.second.get();

      Profile* profile = profile_info->GetRawProfile();
      if (profile && profile->IsSystemProfile())
        continue;

      for (const auto& origin_and_count : profile_info->keep_alives) {
        ProfileKeepAliveOrigin origin = origin_and_count.first;
        int count = origin_and_count.second;
        if (count > 0) {
          UMA_HISTOGRAM_ENUMERATION("Profile.KeepAliveLeakAtShutdown", origin);
        }
      }
    }
  }
}

// static
bool ProfileManager::IsProfileDirectoryMarkedForDeletion(
    const base::FilePath& profile_path) {
  const auto it = ProfilesToDelete().find(profile_path);
  return it != ProfilesToDelete().end() &&
         it->second == ProfileDeletionStage::MARKED;
}

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
// static
void ProfileManager::ShutdownSessionServices() {
  ProfileManager* pm = g_browser_process->profile_manager();
  if (!pm)  // Is nullptr when running unit tests.
    return;
  for (auto* profile : pm->GetLoadedProfiles()) {
    // Don't construct SessionServices for every type just to
    // shut them down. If they were never created, just skip.
    if (SessionServiceFactory::GetForProfileIfExisting(profile))
      SessionServiceFactory::ShutdownForProfile(profile);
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
    if (AppSessionServiceFactory::GetForProfileIfExisting(profile))
      AppSessionServiceFactory::ShutdownForProfile(profile);
#endif
  }
}
#endif

// static
void ProfileManager::NukeDeletedProfilesFromDisk() {
  for (const auto& item : ProfilesToDelete()) {
    if (item.second == ProfileDeletionStage::MARKED)
      NukeProfileFromDiskImpl(item.first, /*retry_count=*/0,
                              /*max_retry_count=*/1, base::OnceClosure());
  }
  ProfilesToDelete().clear();
}

// static
Profile* ProfileManager::GetLastUsedProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Use default login profile if user has not logged in yet.
  if (!IsLoggedIn())
    return profile_manager->GetActiveUserOrOffTheRecordProfile();

  // CrOS multi-profiles implementation is different so GetLastUsedProfile()
  // has custom implementation too.
  base::FilePath profile_dir;
  // In case of multi-profiles we ignore "last used profile" preference
  // since it may refer to profile that has been in use in previous session.
  // That profile dir may not be mounted in this session so instead return
  // active profile from current session.
  profile_dir = chromeos::ProfileHelper::Get()->GetActiveUserProfileDir();

  Profile* profile = profile_manager->GetProfileByPath(
      profile_manager->user_data_dir().Append(profile_dir));

  // Accessing a user profile before it is loaded may lead to policy exploit.
  // See http://crbug.com/689206.
  LOG_IF(FATAL, !profile) << "Calling GetLastUsedProfile() before profile "
                          << "initialization is completed.";

  return profile->IsGuestSession()
             ? profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)
             : profile;
#else
  return profile_manager->GetProfile(profile_manager->GetLastUsedProfileDir());
#endif
}

// static
Profile* ProfileManager::GetLastUsedProfileIfLoaded() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir());
}

// static
Profile* ProfileManager::GetLastUsedProfileAllowedByPolicy() {
  Profile* profile = GetLastUsedProfile();
  if (!profile)
    return nullptr;
  if (IsOffTheRecordModeForced(profile))
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  return profile;
}

// static
bool ProfileManager::IsOffTheRecordModeForced(Profile* profile) {
  return profile->IsGuestSession() || profile->IsSystemProfile() ||
         IncognitoModePrefs::GetAvailability(profile->GetPrefs()) ==
             IncognitoModePrefs::Availability::kForced;
}

// static
std::vector<Profile*> ProfileManager::GetLastOpenedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(profile_manager);
  DCHECK(local_state);

  std::vector<Profile*> to_return;
  if (local_state->HasPrefPath(prefs::kProfilesLastActive) &&
      local_state->GetList(prefs::kProfilesLastActive)) {
    // Make a copy because the list might change in the calls to GetProfile.
    const base::Value profile_list =
        local_state->GetList(prefs::kProfilesLastActive)->Clone();
    for (const auto& entry : profile_list.GetList()) {
      const std::string* profile_base_name = entry.GetIfString();
      if (!profile_base_name || profile_base_name->empty() ||
          *profile_base_name ==
              base::FilePath(chrome::kSystemProfileDir).AsUTF8Unsafe()) {
        LOG(WARNING) << "Invalid entry in " << prefs::kProfilesLastActive;
        continue;
      }
      Profile* profile =
          profile_manager->GetProfile(profile_manager->user_data_dir().Append(
              base::FilePath::FromUTF8Unsafe(*profile_base_name)));
      if (profile) {
        // crbug.com/823338 -> CHECK that the profiles aren't guest or
        // incognito, causing a crash during session restore.
        CHECK(!profile->IsGuestSession())
            << "Guest profiles shouldn't have been saved as active profiles";
        CHECK(!profile->IsOffTheRecord())
            << "OTR profiles shouldn't have been saved as active profiles";
        to_return.push_back(profile);
      }
    }
  }
  return to_return;
}

// static
Profile* ProfileManager::GetPrimaryUserProfile() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsLoggedIn()) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    const user_manager::User* user = manager->GetPrimaryUser();
    if (!user)  // Can be null in unit tests.
      return nullptr;

    // Note: The ProfileHelper will take care of guest profiles.
    return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  }
#endif

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)  // Can be null in unit tests.
    return nullptr;

  return profile_manager->GetActiveUserOrOffTheRecordProfile();
}

// static
Profile* ProfileManager::GetActiveUserProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!profile_manager)
    return nullptr;

  if (IsLoggedIn()) {
    user_manager::UserManager* manager = user_manager::UserManager::Get();
    const user_manager::User* user = manager->GetActiveUser();
    // To avoid an endless loop (crbug.com/334098) we have to additionally check
    // if the profile of the user was already created. If the profile was not
    // yet created we load the profile using the profile directly.
    // TODO: This should be cleaned up with the new profile manager.
    if (user && user->is_profile_created())
      return chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(user);
  }
#endif
  Profile* profile = profile_manager->GetActiveUserOrOffTheRecordProfile();
  // |profile| could be null if the user doesn't have a profile yet and the path
  // is on a read-only volume (preventing Chrome from making a new one).
  // However, most callers of this function immediately dereference the result
  // which would lead to crashes in a variety of call sites. Assert here to
  // figure out how common this is. http://crbug.com/383019
  CHECK(profile) << profile_manager->user_data_dir().AsUTF8Unsafe();
  return profile;
}

// static
Profile* ProfileManager::CreateInitialProfile() {
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      profile_manager->GetProfile(profile_manager->user_data_dir().Append(
          profile_manager->GetInitialProfileDir()));

  if (profile_manager->ShouldGoOffTheRecord(profile))
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  return profile;
}

void ProfileManager::AddObserver(ProfileManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ProfileManager::RemoveObserver(ProfileManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

Profile* ProfileManager::GetProfile(const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::GetProfile");

  // If the profile is already loaded (e.g., chrome.exe launched twice), just
  // return it.
  Profile* profile = GetProfileByPath(profile_dir);
  if (profile)
    return profile;
  return CreateAndInitializeProfile(profile_dir);
}

size_t ProfileManager::GetNumberOfProfiles() {
  return GetProfileAttributesStorage().GetNumberOfProfiles();
}

bool ProfileManager::LoadProfile(const base::FilePath& profile_base_name,
                                 bool incognito,
                                 ProfileLoadedCallback callback) {
  const base::FilePath profile_path = user_data_dir().Append(profile_base_name);
  return LoadProfileByPath(profile_path, incognito, std::move(callback));
}

bool ProfileManager::LoadProfileByPath(const base::FilePath& profile_path,
                                       bool incognito,
                                       ProfileLoadedCallback callback) {
  ProfileAttributesEntry* entry =
      GetProfileAttributesStorage().GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    std::move(callback).Run(nullptr);
    LOG(ERROR) << "Loading a profile path that does not exist";
    return false;
  }
  CreateProfileAsync(
      profile_path,
      base::BindRepeating(&OnProfileLoaded,
                          // OnProfileLoaded may be called multiple times, but
                          // |callback| will be called only once.
                          base::OwnedRef(std::move(callback)), incognito));
  return true;
}

void ProfileManager::CreateProfileAsync(const base::FilePath& profile_path,
                                        const CreateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("browser,startup",
               "ProfileManager::CreateProfileAsync",
               "profile_path",
               profile_path.AsUTF8Unsafe());

  if (!CanCreateProfileAtPath(profile_path)) {
    if (!callback.is_null())
      callback.Run(nullptr, Profile::CREATE_STATUS_LOCAL_FAIL);
    return;
  }

  // Create the profile if needed and collect its ProfileInfo.
  auto iter = profiles_info_.find(profile_path);
  ProfileInfo* info = nullptr;

  if (iter != profiles_info_.end()) {
    info = iter->second.get();
  } else {
    // Initiate asynchronous creation process.
    info = RegisterOwnedProfile(CreateProfileAsyncHelper(profile_path));
  }

  // Call or enqueue the callback.
  if (!callback.is_null()) {
    if (iter != profiles_info_.end() && info->GetCreatedProfile()) {
      Profile* profile = info->GetCreatedProfile();
      // If this was the Guest profile, apply settings and go OffTheRecord.
      // The system profile also needs characteristics of being off the record,
      // such as having no extensions, not writing to disk, etc.
      if (profile->IsGuestSession() || profile->IsSystemProfile()) {
        SetNonPersonalProfilePrefs(profile);
        profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
      }

      // Profile has already been created. Run callback immediately.
      callback.Run(profile, Profile::CREATE_STATUS_INITIALIZED);
    } else {
      // Profile is either already in the process of being created, or new.
      // Add callback to the list.
      info->callbacks.push_back(callback);
    }
  }
}

bool ProfileManager::IsValidProfile(const void* profile) {
  for (auto iter = profiles_info_.begin(); iter != profiles_info_.end();
       ++iter) {
    Profile* candidate = iter->second->GetCreatedProfile();
    if (!candidate)
      continue;
    if (candidate == profile)
      return true;
    std::vector<Profile*> otr_profiles =
        candidate->GetAllOffTheRecordProfiles();
    if (base::Contains(otr_profiles, profile))
      return true;
  }
  return false;
}

base::FilePath ProfileManager::GetInitialProfileDir() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsLoggedIn())
    return chromeos::ProfileHelper::Get()->GetActiveUserProfileDir();
#endif
  base::FilePath relative_profile_dir;
  // TODO(mirandac): should not automatically be default profile.
  return relative_profile_dir.AppendASCII(chrome::kInitialProfile);
}

base::FilePath ProfileManager::GetLastUsedProfileDir() {
  return user_data_dir_.Append(GetLastUsedProfileBaseName());
}

base::FilePath ProfileManager::GetProfileDirForEmail(const std::string& email) {
  for (const auto* entry :
       GetProfileAttributesStorage().GetAllProfilesAttributes()) {
    if (gaia::AreEmailsSame(base::UTF16ToUTF8(entry->GetUserName()), email))
      return entry->GetPath();
  }
  return base::FilePath();
}

std::vector<Profile*> ProfileManager::GetLoadedProfiles() const {
  std::vector<Profile*> profiles;
  for (auto iter = profiles_info_.begin(); iter != profiles_info_.end();
       ++iter) {
    Profile* profile = iter->second->GetCreatedProfile();
    if (profile)
      profiles.push_back(profile);
  }
  return profiles;
}

Profile* ProfileManager::GetProfileByPathInternal(
    const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPathInternal");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->GetRawProfile() : nullptr;
}

bool ProfileManager::IsAllowedProfilePath(const base::FilePath& path) const {
  return path.DirName() == user_data_dir();
}

bool ProfileManager::CanCreateProfileAtPath(const base::FilePath& path) const {
  bool is_allowed_path = IsAllowedProfilePath(path) ||
                         base::CommandLine::ForCurrentProcess()->HasSwitch(
                             switches::kAllowProfilesOutsideUserDir);

  if (!is_allowed_path) {
    LOG(ERROR) << "Cannot create profile at path " << path.AsUTF8Unsafe();
    return false;
  }

  if (IsProfileDirectoryMarkedForDeletion(path))
    return false;

  return true;
}

Profile* ProfileManager::GetProfileByPath(const base::FilePath& path) const {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileByPath");
  ProfileInfo* profile_info = GetProfileInfoByPath(path);
  return profile_info ? profile_info->GetCreatedProfile() : nullptr;
}

// static
Profile* ProfileManager::GetProfileFromProfileKey(ProfileKey* profile_key) {
  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      profile_key->GetPath());
  if (profile->GetProfileKey() == profile_key)
    return profile;

  for (Profile* otr : profile->GetAllOffTheRecordProfiles()) {
    if (otr->GetProfileKey() == profile_key)
      return otr;
  }

  NOTREACHED() << "An invalid profile key is passed.";
  return nullptr;
}

// static
base::FilePath ProfileManager::CreateMultiProfileAsync(
    const std::u16string& name,
    size_t icon_index,
    bool is_hidden,
    const CreateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!name.empty());
  DCHECK(profiles::IsDefaultAvatarIconIndex(icon_index));

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();
  DCHECK(profile_manager->CanCreateProfileAtPath(new_path));

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(new_path);
  if (entry) {
    LOG(WARNING) << "Failed to generate a unique profile name for a new "
                    "profile. Creation parameters will be ignored, loading an "
                    "existing profile at path \""
                 << new_path << "\" instead.";
  } else {
    // Add a storage entry early here to set up a new profile with user selected
    // name and avatar.
    // These parameters will be used to initialize profile's prefs in
    // InitProfileUserPrefs(). AddProfileToStorage() will set any missing
    // attributes after prefs are loaded.
    // TODO(alexilin): consider using the user data to supply these parameters
    // to profile.
    ProfileAttributesInitParams init_params;
    init_params.profile_path = new_path;
    init_params.profile_name = name;
    init_params.icon_index = icon_index;
    init_params.is_ephemeral = is_hidden;
    init_params.is_omitted = is_hidden;
    storage.AddProfile(std::move(init_params));
  }

  profile_manager->CreateProfileAsync(new_path, callback);
  return new_path;
}

// static
base::FilePath ProfileManager::GetGuestProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath guest_path = profile_manager->user_data_dir();
  return guest_path.Append(chrome::kGuestProfileDir);
}

// static
base::FilePath ProfileManager::GetSystemProfilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::FilePath system_path = profile_manager->user_data_dir();
  return system_path.Append(chrome::kSystemProfileDir);
}

base::FilePath ProfileManager::GenerateNextProfileDirectoryPath() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DCHECK(profiles::IsMultipleProfilesEnabled());

  // Create the next profile in the next available directory slot.
  int next_directory = local_state->GetInteger(prefs::kProfilesNumCreated);
  std::string profile_name = chrome::kMultiProfileDirPrefix;
  profile_name.append(base::NumberToString(next_directory));
  base::FilePath new_path = user_data_dir_;
#if defined(OS_WIN)
  new_path = new_path.Append(base::ASCIIToWide(profile_name));
#else
  new_path = new_path.Append(profile_name);
#endif
  local_state->SetInteger(prefs::kProfilesNumCreated, ++next_directory);
  return new_path;
}

ProfileAttributesStorage& ProfileManager::GetProfileAttributesStorage() {
  TRACE_EVENT0("browser", "ProfileManager::GetProfileAttributesStorage");
  if (!profile_attributes_storage_) {
    profile_attributes_storage_ = std::make_unique<ProfileAttributesStorage>(
        g_browser_process->local_state(), user_data_dir_);
  }
  return *profile_attributes_storage_.get();
}

ProfileShortcutManager* ProfileManager::profile_shortcut_manager() {
  return profile_shortcut_manager_.get();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
AccountProfileMapper* ProfileManager::GetAccountProfileMapper() {
  if (!account_profile_mapper_ &&
      base::FeatureList::IsEnabled(kMultiProfileAccountConsistency)) {
    account_profile_mapper_ = std::make_unique<AccountProfileMapper>(
        GetAccountManagerFacade(/*profile_path=*/std::string()),
        &GetProfileAttributesStorage());
  }
  return account_profile_mapper_.get();
}
#endif

#if !defined(OS_ANDROID)
void ProfileManager::MaybeScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    ProfileLoadedCallback callback,
    ProfileMetrics::ProfileDelete deletion_source) {
  if (!ScheduleProfileDirectoryForDeletion(profile_dir))
    return;

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_dir);
  if (entry) {
    storage.RecordDeletedProfileState(entry);
  }
  ProfileMetrics::LogProfileDeleteUser(deletion_source);

  ScheduleProfileForDeletion(profile_dir, std::move(callback));
}

void ProfileManager::ScheduleProfileForDeletion(
    const base::FilePath& profile_dir,
    ProfileLoadedCallback callback) {
  DCHECK(profiles::IsMultipleProfilesEnabled());
  DCHECK(!IsProfileDirectoryMarkedForDeletion(profile_dir));

  Profile* profile = GetProfileByPath(profile_dir);
  if (profile) {
    // Cancel all in-progress downloads before deleting the profile to prevent a
    // "Do you want to exit Google Chrome and cancel the downloads?" prompt
    // (crbug.com/336725).
    DownloadCoreService* service =
        DownloadCoreServiceFactory::GetForBrowserContext(profile);
    service->CancelDownloads();
    DCHECK_EQ(0, service->NonMaliciousDownloadCount());

    // Close all browser windows before deleting the profile. If the user
    // cancels the closing of any tab in an OnBeforeUnload event, profile
    // deletion is also cancelled. (crbug.com/289390)
    BrowserList::CloseAllBrowsersWithProfile(
        profile,
        base::BindRepeating(
            &ProfileManager::EnsureActiveProfileExistsBeforeDeletion,
            base::Unretained(this), base::Passed(std::move(callback))),
        base::BindRepeating(&CancelProfileDeletion), false);
  } else {
    EnsureActiveProfileExistsBeforeDeletion(std::move(callback), profile_dir);
  }
}
#endif  // !defined(OS_ANDROID)

void ProfileManager::AutoloadProfiles() {
  // If running in the background is disabled for the browser, do not autoload
  // any profiles.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  if (!local_state->HasPrefPath(prefs::kBackgroundModeEnabled) ||
      !local_state->GetBoolean(prefs::kBackgroundModeEnabled)) {
    return;
  }

  std::vector<ProfileAttributesEntry*> entries =
      GetProfileAttributesStorage().GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    if (entry->GetBackgroundStatus()) {
      // If status is true, that profile is running background apps. By calling
      // GetProfile, we automatically cause the profile to be loaded which will
      // register it with the BackgroundModeManager.
      GetProfile(entry->GetPath());
    }
  }
}

void ProfileManager::CleanUpEphemeralProfiles() {
  const base::FilePath last_used_profile_base_name =
      GetLastUsedProfileBaseName();
  bool last_active_profile_deleted = false;
  base::FilePath new_profile_path;
  std::vector<base::FilePath> profiles_to_delete;
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (entry->IsEphemeral()) {
      profiles_to_delete.push_back(profile_path);
      RemoveFromLastActiveProfilesPrefList(profile_path);
      if (profile_path.BaseName() == last_used_profile_base_name)
        last_active_profile_deleted = true;
    } else if (new_profile_path.empty()) {
      new_profile_path = profile_path;
    }
  }

  // If the last active profile was ephemeral or all profiles are deleted due to
  // ephemeral, set a new one.
  if (last_active_profile_deleted ||
      (entries.size() == profiles_to_delete.size() &&
       !profiles_to_delete.empty())) {
    if (new_profile_path.empty())
      new_profile_path = GenerateNextProfileDirectoryPath();

    profiles::SetLastUsedProfile(new_profile_path.BaseName());
  }

  for (const base::FilePath& profile_path : profiles_to_delete) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDisk, profile_path,
                       base::OnceClosure()));

    storage.RemoveProfile(profile_path);
  }
}

void ProfileManager::CleanUpDeletedProfiles() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::ListValue* deleted_profiles =
      local_state->GetList(prefs::kProfilesDeleted);
  DCHECK(deleted_profiles);

  for (const base::Value& value : deleted_profiles->GetList()) {
    absl::optional<base::FilePath> profile_path = base::ValueToFilePath(value);
    // Although it should never happen, make sure this is a valid path in the
    // user_data_dir, so we don't accidentally delete something else.
    if (profile_path && IsAllowedProfilePath(*profile_path)) {
      if (base::PathExists(*profile_path)) {
        LOG(WARNING) << "Files of a deleted profile still exist after restart. "
                        "Cleaning up now.";
        base::ThreadPool::PostTask(
            FROM_HERE,
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
            base::BindOnce(&NukeProfileFromDisk, *profile_path,
                           base::BindOnce(&ProfileCleanedUp, value.Clone())));
      } else {
        // Everything is fine, the profile was removed on shutdown.
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&ProfileCleanedUp, value.Clone()));
      }
    } else {
      LOG(ERROR) << "Found invalid profile path in deleted_profiles: "
                 << profile_path->AsUTF8Unsafe();
      NOTREACHED();
    }
  }
}

void ProfileManager::InitProfileUserPrefs(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::InitProfileUserPrefs");
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();

  if (!IsAllowedProfilePath(profile->GetPath())) {
    LOG(WARNING) << "Failed to initialize prefs for a profile at invalid path: "
                 << profile->GetPath().AsUTF8Unsafe();
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // User object may already have changed user type, so we apply that
  // type to profile.
  // If profile type has changed, remove ProfileAttributesEntry for it to make
  // sure it is fully re-initialized later.
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user) {
    const bool user_is_child =
        (user->GetType() == user_manager::USER_TYPE_CHILD);
    const bool profile_is_child = profile->IsChild();
    const bool profile_is_new = profile->IsNewProfile();
    const bool profile_is_managed = !profile->IsOffTheRecord() &&
                                    arc::policy_util::IsAccountManaged(profile);

    if (!profile_is_new && profile_is_child != user_is_child) {
      ProfileAttributesEntry* entry =
          storage.GetProfileAttributesWithPath(profile->GetPath());
      if (entry) {
        LOG(WARNING) << "Profile child status has changed.";
        storage.RemoveProfile(profile->GetPath());
      }
      ash::ChildAccountTypeChangedUserData::GetForProfile(profile)->SetValue(
          true);
    } else {
      ash::ChildAccountTypeChangedUserData::GetForProfile(profile)->SetValue(
          false);
    }

    // Notify ARC about transition via prefs if needed.
    if (!profile_is_new) {
      const bool arc_is_managed =
          profile->GetPrefs()->GetBoolean(arc::prefs::kArcIsManaged);
      const bool arc_is_managed_set =
          profile->GetPrefs()->HasPrefPath(arc::prefs::kArcIsManaged);

      const bool arc_signed_in =
          profile->GetPrefs()->GetBoolean(arc::prefs::kArcSignedIn);

      arc::ArcManagementTransition transition;
      if (!arc_signed_in) {
        // No transition is necessary if user never enabled ARC.
        transition = arc::ArcManagementTransition::NO_TRANSITION;
      } else if (profile_is_child != user_is_child) {
        transition = user_is_child
                         ? arc::ArcManagementTransition::REGULAR_TO_CHILD
                         : arc::ArcManagementTransition::CHILD_TO_REGULAR;
      } else if (profile_is_managed && arc_is_managed_set && !arc_is_managed) {
        transition = arc::ArcManagementTransition::UNMANAGED_TO_MANAGED;
      } else {
        // User state has not changed.
        transition = arc::ArcManagementTransition::NO_TRANSITION;
      }

      profile->GetPrefs()->SetInteger(arc::prefs::kArcManagementTransition,
                                      static_cast<int>(transition));
    }

    if (user_is_child) {
      profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                     supervised_users::kChildAccountSUID);
    } else {
      profile->GetPrefs()->ClearPref(prefs::kSupervisedUserId);
    }
  }
#endif

  size_t avatar_index;
  std::string profile_name;
  std::string supervised_user_id;
  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF8(IDS_PROFILES_GUEST_PROFILE_NAME);
    avatar_index = 0;
  } else {
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    // If the profile attributes storage has an entry for this profile, use the
    // data in the profile attributes storage.
    if (entry) {
      avatar_index = entry->GetAvatarIconIndex();
      profile_name = base::UTF16ToUTF8(entry->GetLocalProfileName());
      supervised_user_id = entry->GetSupervisedUserId();
    } else {
      avatar_index = profiles::GetPlaceholderAvatarIndex();
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
      profile_name =
          base::UTF16ToUTF8(storage.ChooseNameForNewProfile(avatar_index));
#else
      profile_name = l10n_util::GetStringUTF8(IDS_DEFAULT_PROFILE_NAME);
#endif
    }
  }

  // TODO(https://crbug.com/1194607): investigate whether these prefs are
  // actually useful.
  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileAvatarIndex))
    profile->GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, avatar_index);

  if (!profile->GetPrefs()->HasPrefPath(prefs::kProfileName)) {
    profile->GetPrefs()->SetString(prefs::kProfileName, profile_name);
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool force_supervised_user_id =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      g_browser_process->platform_part()
              ->profile_helper()
              ->GetSigninProfileDir() != profile->GetPath() &&
      g_browser_process->platform_part()
              ->profile_helper()
              ->GetLockScreenAppProfilePath() != profile->GetPath() &&
#endif
      command_line->HasSwitch(switches::kSupervisedUserId);

  if (force_supervised_user_id) {
    supervised_user_id =
        command_line->GetSwitchValueASCII(switches::kSupervisedUserId);
  }
  if (force_supervised_user_id ||
      !profile->GetPrefs()->HasPrefPath(prefs::kSupervisedUserId)) {
    profile->GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   supervised_user_id);
  }
#if !defined(OS_ANDROID)
  // TODO(pmonette): Fix IsNewProfile() to handle the case where the profile is
  // new even if the "Preferences" file already existed. (For example: The
  // master_preferences file is dumped into the default profile on first run,
  // before profile creation.)
  if (profile->IsNewProfile() || first_run::IsChromeFirstRun()) {
    profile->GetPrefs()->SetBoolean(prefs::kHasSeenWelcomePage, false);
  }
#endif  // !defined(OS_ANDROID)
}

void ProfileManager::RegisterTestingProfile(std::unique_ptr<Profile> profile,
                                            bool add_to_storage) {
  ProfileInfo* profile_info = RegisterOwnedProfile(std::move(profile));
  profile_info->MarkProfileAsCreated(profile_info->GetRawProfile());
  if (add_to_storage) {
    InitProfileUserPrefs(profile_info->GetCreatedProfile());
    AddProfileToStorage(profile_info->GetCreatedProfile());
  }
}

std::unique_ptr<Profile> ProfileManager::CreateProfileHelper(
    const base::FilePath& path) {
  TRACE_EVENT0("browser", "ProfileManager::CreateProfileHelper");

  return Profile::CreateProfile(path, this, Profile::CREATE_MODE_SYNCHRONOUS);
}

std::unique_ptr<Profile> ProfileManager::CreateProfileAsyncHelper(
    const base::FilePath& path) {
  return Profile::CreateProfile(path, this, Profile::CREATE_MODE_ASYNCHRONOUS);
}

bool ProfileManager::HasKeepAliveForTesting(const Profile* profile,
                                            ProfileKeepAliveOrigin origin) {
  DCHECK(profile);
  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  DCHECK(info);
  return info->keep_alives[origin] > 0;
}

void ProfileManager::AddKeepAlive(const Profile* profile,
                                  ProfileKeepAliveOrigin origin) {
  DCHECK_NE(ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow, origin);
  if (!base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose))
    return;

  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());

  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  if (!info) {
    // Can be null in unit tests, when the Profile was not created via
    // ProfileManager.
    VLOG(1) << "AddKeepAlive(" << profile->GetDebugName() << ", " << origin
            << ") called before the Profile was added to the ProfileManager. "
            << "The keepalive was not added. This may cause a crash during "
            << "teardown. (except in unit tests, where Profiles may not be "
            << "registered with the ProfileManager)";
    return;
  }

  DCHECK_NE(0, GetTotalRefCount(info->keep_alives))
      << "AddKeepAlive() on a soon-to-be-deleted Profile is not allowed";

  info->keep_alives[origin]++;

  VLOG(1) << "AddKeepAlive(" << profile->GetDebugName() << ", " << origin
          << "). keep_alives=" << info->keep_alives;

  if (origin == ProfileKeepAliveOrigin::kBrowserWindow)
    ClearFirstBrowserWindowKeepAlive(profile);
}

void ProfileManager::RemoveKeepAlive(const Profile* profile,
                                     ProfileKeepAliveOrigin origin) {
  DCHECK_NE(ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow, origin);
  if (!base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose))
    return;

  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());

  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  if (!info) {
    // Can be null in unit tests, when the Profile was not created via
    // ProfileManager.
    VLOG(1) << "RemoveKeepAlive(" << profile->GetDebugName() << ", " << origin
            << ") called before the Profile was added to the "
            << "ProfileManager. The keepalive was not removed. (this is OK in "
            << "unit tests, wheres Profile may not be registered with the "
            << "ProfileManager)";
    return;
  }

  DCHECK(base::Contains(info->keep_alives, origin));
  info->keep_alives[origin]--;
  DCHECK_LE(0, info->keep_alives[origin]);

  VLOG(1) << "RemoveKeepAlive(" << profile->GetDebugName() << ", " << origin
          << "). keep_alives=" << info->keep_alives;

  DeleteProfileIfNoKeepAlive(info);
}

void ProfileManager::ClearFirstBrowserWindowKeepAlive(const Profile* profile) {
  DCHECK(base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose));

  DCHECK(profile);
  DCHECK(!profile->IsOffTheRecord());

  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  DCHECK(info);

  int& waiting_for_first_browser_window =
      info->keep_alives[ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow];

  if (waiting_for_first_browser_window == 0)
    return;

  waiting_for_first_browser_window = 0;

  VLOG(1) << "ClearFirstBrowserWindowKeepAlive(" << profile->GetDebugName()
          << "). keep_alives=" << info->keep_alives;

  DeleteProfileIfNoKeepAlive(info);
}

void ProfileManager::DeleteProfileIfNoKeepAlive(const ProfileInfo* info) {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetTotalRefCount(info->keep_alives) != 0)
    return;

  if (!info->GetCreatedProfile()) {
    NOTREACHED() << "Attempted to delete profile "
                 << info->GetRawProfile()->GetDebugName()
                 << " before it was created. This is not valid.";
  }

  VLOG(1) << "Deleting profile " << info->GetCreatedProfile()->GetDebugName();
  RemoveProfile(info->GetCreatedProfile()->GetPath());
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

void ProfileManager::DoFinalInit(ProfileInfo* profile_info,
                                 bool go_off_the_record) {
  TRACE_EVENT0("browser", "ProfileManager::DoFinalInit");

  Profile* profile = profile_info->GetRawProfile();
  DoFinalInitForServices(profile, go_off_the_record);
  AddProfileToStorage(profile);
  DoFinalInitLogging(profile);

  // Set the |created| flag now so that PROFILE_ADDED handlers can use
  // GetProfileByPath().
  //
  // TODO(nicolaso): De-spaghettify MarkProfileAsCreated() by only calling it
  // here, and nowhere else.
  profile_info->MarkProfileAsCreated(profile);

  for (auto& observer : observers_)
    observer.OnProfileAdded(profile);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_ADDED,
      content::Source<Profile>(profile),
      content::NotificationService::NoDetails());

  // At this point, the user policy service and the child account service
  // had enough time to initialize and should have updated the user signout
  // flag attached to the profile.
  signin_util::EnsureUserSignoutAllowedIsInitializedForProfile(profile);
  signin_util::EnsurePrimaryAccountAllowedForProfile(profile);

#if !defined(OS_ANDROID)
  // The caret browsing command-line switch toggles caret browsing on
  // initially, but the user can still toggle it from there.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCaretBrowsing)) {
    profile->GetPrefs()->SetBoolean(prefs::kCaretBrowsingEnabled, true);
  }
#endif  // !defined(OS_ANDROID)

  // Delete browsing data specified by the ClearBrowsingDataOnExitList policy
  // if they were not properly deleted on the last browser shutdown.
  auto* browsing_data_lifetime_manager =
      ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(profile);
  if (browsing_data_lifetime_manager && !profile->IsOffTheRecord() &&
      profile->GetPrefs()->GetBoolean(
          browsing_data::prefs::kClearBrowsingDataOnExitDeletionPending)) {
    browsing_data_lifetime_manager->ClearBrowsingDataForOnExitPolicy(
        /*keep_browser_alive=*/false);
  }
}

void ProfileManager::DoFinalInitForServices(Profile* profile,
                                            bool go_off_the_record) {
  if (!do_final_services_init_)
    return;

  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitForServices");

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool extensions_enabled = !go_off_the_record;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableLoginScreenApps) &&
      chromeos::ProfileHelper::IsSigninProfile(profile)) {
    extensions_enabled = true;
  }
  if (chromeos::ProfileHelper::IsLockScreenAppProfile(profile))
    extensions_enabled = true;
#endif
  extensions::ExtensionSystem::Get(profile)->InitForRegularProfile(
      extensions_enabled);

  // Set the block extensions bit on the ExtensionService. There likely are no
  // blockable extensions to block.
  ProfileAttributesEntry* entry =
      GetProfileAttributesStorage().GetProfileAttributesWithPath(
          profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->BlockAllExtensions();
  }

#endif
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Initialization needs to happen after extension system initialization (for
  // extension::ManagementPolicy) and InitProfileUserPrefs (for setting the
  // initializing the supervised flag if necessary).
  ChildAccountServiceFactory::GetForProfile(profile)->Init();
  SupervisedUserServiceFactory::GetForProfile(profile)->Init();
#endif

  // Activate data reduction proxy. This creates a request context and makes a
  // URL request to check if the data reduction proxy server is reachable.
  DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile)->
      MaybeActivateDataReductionProxy(true);

  // Ensure NavigationPredictorKeyedService is started.
  NavigationPredictorKeyedServiceFactory::GetForProfile(profile);

  IdentityManagerFactory::GetForProfile(profile)->OnNetworkInitialized();
  AccountReconcilorFactory::GetForProfile(profile);

  // Initialization needs to happen after the browser context is available
  // because SyncService needs the URL context getter.
  UnifiedConsentServiceFactory::GetForProfile(profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!chromeos::ProfileHelper::IsSigninProfile(profile))
    captions::LiveCaptionControllerFactory::GetForProfile(profile)->Init();
#elif !defined(OS_ANDROID)  // !OS_ANDROID && !IS_CHROMEOS_ASH
  captions::LiveCaptionControllerFactory::GetForProfile(profile)->Init();
#endif

#if defined(OS_WIN) && BUILDFLAG(ENABLE_DICE_SUPPORT)
  signin_util::SigninWithCredentialProviderIfPossible(profile);
#endif

  AccessibilityLabelsServiceFactory::GetForProfile(profile)->Init();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManagerPolicyControllerFactory::GetForBrowserContext(profile);
#endif

  // Creates the LiteVideo Keyed Service and begins loading the
  // hint cache and user blocklist.
  auto* lite_video_keyed_service =
      LiteVideoKeyedServiceFactory::GetForProfile(profile);
  if (lite_video_keyed_service)
    lite_video_keyed_service->Initialize(profile->GetPath());

  // TODO(crbug.com/1031477): Remove once getting this created with the browser
  // context does not change dependency initialization order to cause crashes.
  AdaptiveQuietNotificationPermissionUiEnabler::GetForProfile(profile);
}

void ProfileManager::DoFinalInitLogging(Profile* profile) {
  if (!do_final_services_init_)
    return;

  TRACE_EVENT0("browser", "ProfileManager::DoFinalInitLogging");
  // Count number of extensions in this profile.
  int enabled_app_count = -1;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  enabled_app_count = GetEnabledAppCount(profile);
#endif

  // Log the profile size after a reasonable startup delay.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ProfileSizeTask, profile->GetPath(), enabled_app_count),
      base::Seconds(112));
}

ProfileManager::ProfileInfo::ProfileInfo() {
  // The profile should have a refcount >=1 until AddKeepAlive() is called.
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose))
    keep_alives[ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow] = 1;
}

ProfileManager::ProfileInfo::~ProfileInfo() {
  // Regardless of sync or async creation, we always take ownership right after
  // Profile::CreateProfile(). So we should always own the Profile by this
  // point.
  DCHECK(owned_profile_);
  DCHECK_EQ(owned_profile_.get(), unowned_profile_);
  unowned_profile_ = nullptr;
  ProfileDestroyer::DestroyProfileWhenAppropriate(owned_profile_.release());
}

// static
std::unique_ptr<ProfileManager::ProfileInfo>
ProfileManager::ProfileInfo::FromUnownedProfile(Profile* profile) {
  // ProfileInfo's constructor is private, can't make_unique().
  std::unique_ptr<ProfileInfo> info(new ProfileInfo());
  info->unowned_profile_ = profile;
  return info;
}

void ProfileManager::ProfileInfo::TakeOwnershipOfProfile(
    std::unique_ptr<Profile> profile) {
  DCHECK_EQ(unowned_profile_, profile.get());
  DCHECK(!owned_profile_);
  owned_profile_ = std::move(profile);
}

void ProfileManager::ProfileInfo::MarkProfileAsCreated(Profile* profile) {
  DCHECK_EQ(GetRawProfile(), profile);
  created_ = true;
}

Profile* ProfileManager::ProfileInfo::GetCreatedProfile() const {
  return created_ ? GetRawProfile() : nullptr;
}

Profile* ProfileManager::ProfileInfo::GetRawProfile() const {
  DCHECK(owned_profile_ == nullptr || owned_profile_.get() == unowned_profile_);
  return unowned_profile_;
}

Profile* ProfileManager::GetActiveUserOrOffTheRecordProfile() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!IsLoggedIn()) {
    base::FilePath default_profile_dir =
        profiles::GetDefaultProfileDir(user_data_dir_);
    Profile* profile = GetProfile(default_profile_dir);
    // For cros, return the OTR profile so we never accidentally keep
    // user data in an unencrypted profile. But doing this makes
    // many of the browser and ui tests fail. We do return the OTR profile
    // if the login-profile switch is passed so that we can test this.
    if (ShouldGoOffTheRecord(profile))
      return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    DCHECK(!user_manager::UserManager::Get()->IsLoggedInAsGuest());
    return profile;
  }

  base::FilePath default_profile_dir =
      user_data_dir_.Append(GetInitialProfileDir());
  ProfileInfo* profile_info = GetProfileInfoByPath(default_profile_dir);
  // Fallback to default off-the-record profile, if user profile has not started
  // loading or has not fully loaded yet.
  if (!profile_info || !profile_info->GetCreatedProfile())
    default_profile_dir = profiles::GetDefaultProfileDir(user_data_dir_);

  Profile* profile = GetProfile(default_profile_dir);
  // Some unit tests didn't initialize the UserManager.
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->IsLoggedInAsGuest())
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  return profile;
#else
  base::FilePath default_profile_dir =
      user_data_dir_.Append(GetInitialProfileDir());
  return GetProfile(default_profile_dir);
#endif
}

bool ProfileManager::AddProfile(std::unique_ptr<Profile> profile) {
  TRACE_EVENT0("browser", "ProfileManager::AddProfile");

  DCHECK(profile);

  // Make sure that we're not loading a profile with the same ID as a profile
  // that's already loaded.
  if (GetProfileByPathInternal(profile->GetPath())) {
    NOTREACHED() << "Attempted to add profile with the same path ("
                 << profile->GetPath().value()
                 << ") as an already-loaded profile.";
    return false;
  }

  ProfileInfo* profile_info = RegisterOwnedProfile(std::move(profile));
  profile_info->MarkProfileAsCreated(profile_info->GetRawProfile());

  InitProfileUserPrefs(profile_info->GetCreatedProfile());
  DoFinalInit(profile_info,
              ShouldGoOffTheRecord(profile_info->GetCreatedProfile()));
  return true;
}

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ProfileManager::RemoveProfile(const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::RemoveProfile");

  DCHECK(base::Contains(profiles_info_, profile_dir));

  Profile* profile = GetProfileByPath(profile_dir);
  bool ephemeral = IsEphemeral(profile);
  bool marked_for_deletion = IsProfileDirectoryMarkedForDeletion(profile_dir);

  // Remove from |profiles_info_|, eventually causing the Profile object's
  // destruction.
  profiles_info_.erase(profile_dir);

  if (!ephemeral && !marked_for_deletion)
    return;

  // If the profile is ephemeral or deleted via ScheduleProfileForDeletion(),
  // also do some cleanup.

  // TODO(crbug.com/88586): There could still be pending tasks that write to
  // disk, and don't need the Profile. If they run after
  // NukeProfileFromDisk(), they may still leave files behind.
  //
  // TODO(crbug.com/1191455): This can also fail if an object is holding a lock
  // to a file in the profile directory. This happens flakily, e.g. with the
  // LevelDB for GCMStore. The locked files don't get deleted properly.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&NukeProfileFromDisk, profile_dir, base::OnceClosure()));
}
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

Profile* ProfileManager::CreateAndInitializeProfile(
    const base::FilePath& profile_dir) {
  TRACE_EVENT0("browser", "ProfileManager::CreateAndInitializeProfile");

  if (!CanCreateProfileAtPath(profile_dir)) {
    LOG(ERROR) << "Cannot create profile at path "
               << profile_dir.AsUTF8Unsafe();
    return nullptr;
  }

  // CHECK that we are not trying to load the same profile twice, to prevent
  // profile corruption. Note that this check also covers the case when we have
  // already started loading the profile but it is not fully initialized yet,
  // which would make Bad Things happen if we returned it.
  CHECK(!GetProfileByPathInternal(profile_dir));
  std::unique_ptr<Profile> profile = CreateProfileHelper(profile_dir);
  if (!profile)
    return nullptr;

  // Place the unique_ptr inside ProfileInfo, which was added by
  // OnProfileCreationStarted().
  ProfileInfo* info = GetProfileInfoByPath(profile->GetPath());
  DCHECK(info);
  info->TakeOwnershipOfProfile(std::move(profile));
  info->MarkProfileAsCreated(info->GetRawProfile());
  Profile* profile_ptr = info->GetCreatedProfile();

  if (profile_ptr->IsGuestSession() || profile_ptr->IsSystemProfile())
    SetNonPersonalProfilePrefs(profile_ptr);

  bool go_off_the_record = ShouldGoOffTheRecord(profile_ptr);
  DoFinalInit(info, go_off_the_record);
  return profile_ptr;
}

void ProfileManager::OnProfileCreationFinished(Profile* profile,
                                               Profile::CreateMode create_mode,
                                               bool success,
                                               bool is_new_profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = profiles_info_.find(profile->GetPath());
  DCHECK(iter != profiles_info_.end());
  ProfileInfo* info = iter->second.get();

  if (create_mode == Profile::CREATE_MODE_SYNCHRONOUS) {
    // Already initialized in OnProfileCreationStarted().
    // TODO(nicolaso): Figure out why this would break browser tests:
    //     DCHECK_EQ(profile, profiles_info_->GetCreatedProfile());
    return;
  }

  std::vector<CreateCallback> callbacks;
  info->callbacks.swap(callbacks);

  // Invoke CREATED callback for normal profiles.
  bool go_off_the_record = ShouldGoOffTheRecord(profile);
  if (success && !go_off_the_record)
    RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);

  // Perform initialization.
  if (success) {
    DoFinalInit(info, go_off_the_record);
    if (go_off_the_record)
      profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  } else {
    profile = nullptr;
    profiles_info_.erase(iter);
  }

  if (profile) {
    // If this was the guest or system profile, finish setting its special
    // status.
    if (profile->IsGuestSession() || profile->IsSystemProfile())
      SetNonPersonalProfilePrefs(profile);

    // Invoke CREATED callback for incognito profiles.
    if (go_off_the_record)
      RunCallbacks(callbacks, profile, Profile::CREATE_STATUS_CREATED);
  }

  // Invoke INITIALIZED or FAIL for all profiles.
  RunCallbacks(callbacks, profile,
               profile ? Profile::CREATE_STATUS_INITIALIZED
                       : Profile::CREATE_STATUS_LOCAL_FAIL);
}

void ProfileManager::OnProfileCreationStarted(Profile* profile,
                                              Profile::CreateMode create_mode) {
  if (create_mode == Profile::CREATE_MODE_ASYNCHRONOUS) {
    // Profile will be registered later, in CreateProfileAsync().
    return;
  }

  if (profiles_info_.find(profile->GetPath()) != profiles_info_.end())
    return;

  // Make sure the Profile is in |profiles_info_| early enough during Profile
  // initialization.
  RegisterUnownedProfile(profile);
}

#if !defined(OS_ANDROID)
void ProfileManager::EnsureActiveProfileExistsBeforeDeletion(
    ProfileLoadedCallback callback,
    const base::FilePath& profile_dir) {
  // In case we delete non-active profile and current profile is valid, proceed.
  const base::FilePath last_used_profile_path = GetLastUsedProfileDir();
  const base::FilePath guest_profile_path = GetGuestProfilePath();
  Profile* last_used_profile = GetProfileByPath(last_used_profile_path);
  if (last_used_profile_path != profile_dir &&
      last_used_profile_path != guest_profile_path && last_used_profile) {
    FinishDeletingProfile(profile_dir, last_used_profile_path);
    return;
  }

  // Search for an active browser and use its profile as active if possible.
  for (Browser* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    base::FilePath cur_path = profile->GetPath();
    if (cur_path != profile_dir && cur_path != guest_profile_path &&
        !IsProfileDirectoryMarkedForDeletion(cur_path)) {
      OnNewActiveProfileLoaded(profile_dir, cur_path, &callback, profile,
                               Profile::CREATE_STATUS_INITIALIZED);
      return;
    }
  }

  // There no valid browsers to fallback, search for any existing valid profile.
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  base::FilePath fallback_profile_path;
  std::vector<ProfileAttributesEntry*> entries =
      storage.GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath cur_path = entry->GetPath();
    // Make sure that this profile is not pending deletion.
    if (cur_path != profile_dir &&
        cur_path != guest_profile_path &&
        !IsProfileDirectoryMarkedForDeletion(cur_path)) {
      fallback_profile_path = cur_path;
      break;
    }
  }

  // If we're deleting the last profile, then create a new profile in its place.
  // Load existing profile otherwise.
  if (fallback_profile_path.empty()) {
    fallback_profile_path = GenerateNextProfileDirectoryPath();
    // A new profile about to be created.
    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_USER_LAST_DELETED);
  }

  // Create and/or load fallback profile.
  CreateProfileAsync(
      fallback_profile_path,
      base::BindRepeating(
          &ProfileManager::OnNewActiveProfileLoaded, base::Unretained(this),
          profile_dir, fallback_profile_path,
          // OnNewActiveProfileLoaded may be called several times, but
          // only once with CREATE_STATUS_INITIALIZED.
          base::Owned(
              std::make_unique<ProfileLoadedCallback>(std::move(callback)))));
}

void ProfileManager::OnLoadProfileForProfileDeletion(
    const base::FilePath& profile_dir,
    Profile* profile) {
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();

  if (!IsProfileDirectoryMarkedForDeletion(profile_dir)) {
    // Ensure RemoveProfile() knows to nuke the profile directory after it's
    // done.
    MarkProfileDirectoryForDeletion(profile_dir);
  }

  if (profile) {
    // TODO(estade): Migrate additional code in this block to observe
    // ProfileManager instead of handling shutdown here.
    for (auto& observer : observers_)
      observer.OnProfileMarkedForPermanentDeletion(profile);

    // Disable sync for doomed profile.
    if (SyncServiceFactory::HasSyncService(profile)) {
      syncer::SyncService* sync_service =
          SyncServiceFactory::GetForProfile(profile);
      // Ensure data is cleared even if sync was already off.
      sync_service->StopAndClear();
    }

    // Some platforms store passwords in keychains. They should be removed.
    scoped_refptr<password_manager::PasswordStoreInterface> password_store =
        PasswordStoreFactory::GetForProfile(profile,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();
    if (password_store.get()) {
      password_store->RemoveLoginsCreatedBetween(
          base::Time(), base::Time::Max(), base::DoNothing());
    }

    // The Profile Data doesn't get wiped until Chrome closes. Since we promised
    // that the user's data would be removed, do so immediately.
    //
    // With DestroyProfileOnBrowserClose, this adds a KeepAlive. So the Profile*
    // only gets deleted *after* browsing data is removed. This also clears some
    // keepalives in the process, e.g. due to background extensions getting
    // uninstalled.
    profiles::RemoveBrowsingDataForProfile(profile_dir);

    // Clean-up pref data that won't be cleaned up by deleting the profile dir.
    profile->GetPrefs()->OnStoreDeletionFromDisk();

  } else {
    // We failed to load the profile, but it's safe to delete a not yet loaded
    // Profile from disk.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&NukeProfileFromDisk, profile_dir, base::OnceClosure()));
  }

  storage.RemoveProfile(profile_dir);

  if (profile &&
      base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    // Allow the Profile* to be deleted, even if it had no browser windows.
    ClearFirstBrowserWindowKeepAlive(profile);
  }
}

void ProfileManager::FinishDeletingProfile(
    const base::FilePath& profile_dir,
    const base::FilePath& new_active_profile_dir) {
  // Update the last used profile pref before closing browser windows. This
  // way the correct last used profile is set for any notification observers.
  profiles::SetLastUsedProfile(new_active_profile_dir.BaseName());

  // Attempt to load the profile before deleting it to properly clean up
  // profile-specific data stored outside the profile directory.
  LoadProfileByPath(
      profile_dir, false,
      base::BindOnce(&ProfileManager::OnLoadProfileForProfileDeletion,
                     base::Unretained(this), profile_dir));
  if (!IsProfileDirectoryMarkedForDeletion(profile_dir)) {
    // Prevents CreateProfileAsync from re-creating the profile.
    MarkProfileDirectoryForDeletion(profile_dir);
  }
}

absl::optional<base::FilePath> ProfileManager::FindLastActiveProfile(
    base::RepeatingCallback<bool(ProfileAttributesEntry*)> predicate) {
  bool found_entry_loaded = false;
  ProfileAttributesEntry* found_entry = nullptr;
  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
    // Skip all profiles forbidden to rollback.
    base::FilePath entry_path = entry->GetPath();
    if (!predicate.Run(entry) || entry_path == GetGuestProfilePath() ||
        IsProfileDirectoryMarkedForDeletion(entry_path))
      continue;
    // Check if |entry| preferable over |found_entry|.
    bool entry_loaded = !!GetProfileByPath(entry_path);
    if (!found_entry || (!found_entry_loaded && entry_loaded) ||
        found_entry->GetActiveTime() < entry->GetActiveTime()) {
      found_entry = entry;
      found_entry_loaded = entry_loaded;
    }
  }
  return found_entry ? absl::optional<base::FilePath>(found_entry->GetPath())
                     : absl::nullopt;
}

// static
void ProfileManager::CleanUpGuestProfile() {
// ChromeOS handles guest data independently.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfileManager* profile_manager = g_browser_process->profile_manager();

  Profile* profile =
      profile_manager->GetProfileByPath(profile_manager->GetGuestProfilePath());
  if (profile) {
    // Clear all browsing data once a Guest Session completes. The Guest
    // profile has BrowserContextKeyedServices that the ProfileDestroyer
    // can't delete it properly.
    profiles::RemoveBrowsingDataForProfile(GetGuestProfilePath());
  }
#endif  //! BUILDFLAG(IS_CHROMEOS_ASH)
}

#endif  // !defined(OS_ANDROID)

ProfileManager::ProfileInfo* ProfileManager::RegisterOwnedProfile(
    std::unique_ptr<Profile> profile) {
  TRACE_EVENT0("browser", "ProfileManager::RegisterOwnedProfile");
  Profile* profile_ptr = profile.get();
  auto info = ProfileInfo::FromUnownedProfile(profile_ptr);
  info->TakeOwnershipOfProfile(std::move(profile));
  ProfileInfo* info_raw = info.get();
  profiles_info_.insert(
      std::make_pair(profile_ptr->GetPath(), std::move(info)));
  return info_raw;
}

ProfileManager::ProfileInfo* ProfileManager::RegisterUnownedProfile(
    Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::RegisterUnownedProfile");
  base::FilePath path = profile->GetPath();
  auto info = ProfileInfo::FromUnownedProfile(profile);
  ProfileInfo* info_raw = info.get();
  profiles_info_.insert(std::make_pair(path, std::move(info)));
  return info_raw;
}

ProfileManager::ProfileInfo* ProfileManager::GetProfileInfoByPath(
    const base::FilePath& path) const {
  auto it = profiles_info_.find(path);
  return it != profiles_info_.end() ? it->second.get() : nullptr;
}

void ProfileManager::AddProfileToStorage(Profile* profile) {
  TRACE_EVENT0("browser", "ProfileManager::AddProfileToCache");
  if (profile->IsGuestSession() || profile->IsSystemProfile())
    return;
  if (!IsAllowedProfilePath(profile->GetPath())) {
    LOG(WARNING) << "Failed to add to storage a profile at invalid path: "
                 << profile->GetPath().AsUTF8Unsafe();
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool is_consented_primary_account =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  std::u16string username = base::UTF8ToUTF16(account_info.email);

  ProfileAttributesStorage& storage = GetProfileAttributesStorage();
  // |entry| below is put inside a pair of brackets for scoping, to avoid
  // potential clashes of variable names.
  {
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile->GetPath());
    if (entry) {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
      bool could_be_managed_status = entry->CanBeManaged();
#endif
      // The ProfileAttributesStorage's info must match the Identity Manager.
      entry->SetAuthInfo(account_info.gaia, username,
                         is_consented_primary_account);

      entry->SetSignedInWithCredentialProvider(profile->GetPrefs()->GetBoolean(
          prefs::kSignedInWithCredentialProvider));

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
      // Sign out if force-sign-in policy is enabled and profile is not signed
      // in.
      VLOG(1) << "ForceSigninCheck: " << signin_util::IsForceSigninEnabled()
              << ", " << could_be_managed_status << ", "
              << !entry->CanBeManaged();
      if (signin_util::IsForceSigninEnabled() && could_be_managed_status &&
          !entry->CanBeManaged()) {
        auto* account_mutator = identity_manager->GetPrimaryAccountMutator();

        // GetPrimaryAccountMutator() returns nullptr on ChromeOS only.
        DCHECK(account_mutator);
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                base::IgnoreResult(
                    &signin::PrimaryAccountMutator::ClearPrimaryAccount),
                base::Unretained(account_mutator),
                signin_metrics::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
                signin_metrics::SignoutDelete::kIgnoreMetric));
      }
#endif
      return;
    }
  }

  ProfileAttributesInitParams init_params;
  init_params.profile_path = profile->GetPath();

  // Profile name and avatar are set by InitProfileUserPrefs and stored in the
  // profile. Use those values to setup the entry in profile attributes storage.
  init_params.profile_name =
      base::UTF8ToUTF16(profile->GetPrefs()->GetString(prefs::kProfileName));

  init_params.icon_index =
      profile->GetPrefs()->GetInteger(prefs::kProfileAvatarIndex);

  init_params.supervised_user_id =
      profile->GetPrefs()->GetString(prefs::kSupervisedUserId);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user)
    init_params.account_id = user->GetAccountId();
#endif

  init_params.gaia_id = account_info.gaia;
  init_params.user_name = username;
  init_params.is_consented_primary_account = is_consented_primary_account;

  init_params.is_ephemeral = IsEphemeral(profile);
  init_params.is_signed_in_with_credential_provider =
      profile->GetPrefs()->GetBoolean(prefs::kSignedInWithCredentialProvider);

  storage.AddProfile(std::move(init_params));
}

void ProfileManager::SetNonPersonalProfilePrefs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kSigninAllowed, false);
  prefs->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled, false);
  prefs->SetBoolean(bookmarks::prefs::kShowBookmarkBar, false);
  prefs->ClearPref(DefaultSearchManager::kDefaultSearchProviderDataPrefName);
}

bool ProfileManager::ShouldGoOffTheRecord(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!chromeos::ProfileHelper::IsRegularProfile(profile)) {
    return true;
  }
#endif
  return profile->IsGuestSession() || profile->IsSystemProfile();
}

void ProfileManager::RunCallbacks(const std::vector<CreateCallback>& callbacks,
                                  Profile* profile,
                                  Profile::CreateStatus status) {
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(profile, status);
}

void ProfileManager::SaveActiveProfiles() {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  ListPrefUpdate update(local_state, prefs::kProfilesLastActive);
  base::ListValue* profile_list = update.Get();

  profile_list->ClearList();

  // crbug.com/120112 -> several non-off-the-record profiles might have the same
  // GetBaseName(). In that case, we cannot restore both
  // profiles. Include each base name only once in the last active profile
  // list.
  std::set<base::FilePath> profile_paths;
  std::vector<Profile*>::const_iterator it;
  for (it = active_profiles_.begin(); it != active_profiles_.end(); ++it) {
    // crbug.com/823338 -> CHECK that the profiles aren't guest or incognito,
    // causing a crash during session restore.
    CHECK((!(*it)->IsGuestSession()))
        << "Guest profiles shouldn't be saved as active profiles";
    CHECK(!(*it)->IsOffTheRecord())
        << "OTR profiles shouldn't be saved as active profiles";
    base::FilePath profile_path = (*it)->GetBaseName();
    // Some profiles might become ephemeral after they are created.
    // Don't persist the System Profile as one of the last actives, it should
    // never get a browser.
    if (!IsEphemeral(*it) &&
        profile_paths.find(profile_path) == profile_paths.end() &&
        profile_path != base::FilePath(chrome::kSystemProfileDir)) {
      profile_paths.insert(profile_path);
      profile_list->Append(profile_path.AsUTF8Unsafe());
    }
  }
}

#if !defined(OS_ANDROID)
void ProfileManager::OnBrowserOpened(Browser* browser) {
  DCHECK(browser);
  Profile* profile = browser->profile();
  DCHECK(profile);
  if (!profile->IsOffTheRecord() && !IsEphemeral(profile) &&
      !browser->is_type_app() && ++browser_counts_[profile] == 1) {
    active_profiles_.push_back(profile);
    SaveActiveProfiles();
  }
  // If browsers are opening, we can't be closing all the browsers. This
  // can happen if the application was exited, but background mode or
  // packaged apps prevented the process from shutting down, and then
  // a new browser window was opened.
  closing_all_browsers_ = false;
}

void ProfileManager::OnBrowserClosed(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);
  if (!profile->IsOffTheRecord() && !browser->is_type_app() &&
      --browser_counts_[profile] == 0) {
    active_profiles_.erase(
        std::find(active_profiles_.begin(), active_profiles_.end(), profile));
    if (!closing_all_browsers_)
      SaveActiveProfiles();
  }

  Profile* original_profile = profile->GetOriginalProfile();
  // Do nothing if the closed window is not the last window of the same profile.
  for (auto* browser_iter : *BrowserList::GetInstance()) {
    if (browser_iter->profile()->GetOriginalProfile() == original_profile)
      return;
  }

  if (profile->IsGuestSession()) {
    auto duration = base::Time::Now() - profile->GetCreationTime();
    base::UmaHistogramCustomCounts("Profile.Guest.OTR.Lifetime",
                                   duration.InMinutes(), 1,
                                   base::Days(28).InMinutes(), 100);

    CleanUpGuestProfile();
  }

  base::FilePath path = profile->GetPath();
  if (IsProfileDirectoryMarkedForDeletion(path)) {
    // Do nothing if the profile is already being deleted.
  } else if (IsEphemeral(profile)) {
    // Avoid scheduling deletion if it's a testing profile that is not
    // registered with profile manager.
    if (profile->AsTestingProfile() &&
        !IsRegisteredAsEphemeral(&GetProfileAttributesStorage(), path)) {
      return;
    }
    // Delete if the profile is an ephemeral profile.
    ScheduleForcedEphemeralProfileForDeletion(path);
  } else if (!profile->IsOffTheRecord()) {
    auto* browsing_data_lifetime_manager =
        ChromeBrowsingDataLifetimeManagerFactory::GetForProfile(
            original_profile);
    if (browsing_data_lifetime_manager) {
      // Delete browsing data set by the ClearBrowsingDataOnExitList policy.
      browsing_data_lifetime_manager->ClearBrowsingDataForOnExitPolicy(
          /*keep_browser_alive=*/true);
    }
  }
}

void ProfileManager::UpdateLastUser(Profile* last_active) {
  // The profile may incorrectly become "active" during its destruction, caused
  // by the UI teardown. See https://crbug.com/1073451
  if (IsProfileDirectoryMarkedForDeletion(last_active->GetPath()))
    return;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  // Only keep track of profiles that we are managing; tests may create others.
  // Also never consider the SystemProfile as "active".
  if (profiles_info_.find(last_active->GetPath()) != profiles_info_.end() &&
      !last_active->IsSystemProfile()) {
    base::FilePath profile_path_base = last_active->GetBaseName();
    if (profile_path_base != GetLastUsedProfileBaseName())
      profiles::SetLastUsedProfile(profile_path_base);

    ProfileAttributesEntry* entry =
        GetProfileAttributesStorage().GetProfileAttributesWithPath(
            last_active->GetPath());
    if (entry) {
      entry->SetActiveTimeToNow();
    }
  }
}

ProfileManager::BrowserListObserver::BrowserListObserver(
    ProfileManager* manager)
    : profile_manager_(manager) {
  BrowserList::AddObserver(this);
}

ProfileManager::BrowserListObserver::~BrowserListObserver() {
  BrowserList::RemoveObserver(this);
}

void ProfileManager::BrowserListObserver::OnBrowserAdded(Browser* browser) {
  profile_manager_->OnBrowserOpened(browser);
}

void ProfileManager::BrowserListObserver::OnBrowserRemoved(
    Browser* browser) {
  profile_manager_->OnBrowserClosed(browser);
}

void ProfileManager::BrowserListObserver::OnBrowserSetLastActive(
    Browser* browser) {
  // If all browsers are being closed (e.g. the user is in the process of
  // shutting down), this event will be fired after each browser is
  // closed. This does not represent a user intention to change the active
  // browser so is not handled here.
  if (profile_manager_->closing_all_browsers_)
    return;

  Profile* last_active = browser->profile();

  // Don't remember ephemeral profiles as last because they are not going to
  // persist after restart.
  if (IsEphemeral(last_active))
    return;

  profile_manager_->UpdateLastUser(last_active);
}

void ProfileManager::OnNewActiveProfileLoaded(
    const base::FilePath& profile_to_delete_path,
    const base::FilePath& new_active_profile_path,
    ProfileLoadedCallback* callback,
    Profile* loaded_profile,
    Profile::CreateStatus status) {
  DCHECK_NE(status, Profile::CREATE_STATUS_LOCAL_FAIL);

  // Only run the code if the profile initialization has finished completely.
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  if (IsProfileDirectoryMarkedForDeletion(new_active_profile_path)) {
    // If the profile we tried to load as the next active profile has been
    // deleted, then retry deleting this profile to redo the logic to load
    // the next available profile.
    EnsureActiveProfileExistsBeforeDeletion(std::move(*callback),
                                            profile_to_delete_path);
    return;
  }

  FinishDeletingProfile(profile_to_delete_path, new_active_profile_path);
  std::move(*callback).Run(loaded_profile);
}

void ProfileManager::ScheduleForcedEphemeralProfileForDeletion(
    const base::FilePath& profile_dir) {
  DCHECK_EQ(0u, chrome::GetBrowserCount(GetProfileByPath(profile_dir)));
  DCHECK(IsRegisteredAsEphemeral(&GetProfileAttributesStorage(), profile_dir));

  absl::optional<base::FilePath> new_active_profile_dir =
      FindLastActiveProfile(base::BindRepeating(
          [](const base::FilePath& profile_dir, ProfileAttributesEntry* entry) {
            return entry->GetPath() != profile_dir;
          },
          profile_dir));
  if (!new_active_profile_dir.has_value())
    new_active_profile_dir = GenerateNextProfileDirectoryPath();
  DCHECK(!new_active_profile_dir->empty());
  RemoveFromLastActiveProfilesPrefList(profile_dir);

  FinishDeletingProfile(profile_dir, new_active_profile_dir.value());
}

void ProfileManager::OnClosingAllBrowsersChanged(bool closing) {
  // Save active profiles when the browser begins shutting down, or if shutdown
  // is cancelled. The active profiles won't be changed during the shutdown
  // process as windows are closed.
  closing_all_browsers_ = closing;
  SaveActiveProfiles();
}
#endif  // !defined(OS_ANDROID)

ProfileManagerWithoutInit::ProfileManagerWithoutInit(
    const base::FilePath& user_data_dir) : ProfileManager(user_data_dir) {
  set_do_final_services_init(false);
}
