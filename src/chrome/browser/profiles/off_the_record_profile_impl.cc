// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/background_sync/background_sync_controller_factory.h"
#include "chrome/browser/background_sync/background_sync_controller_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/client_hints/client_hints_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dom_distiller/profile_utils.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service.h"
#include "chrome/browser/heavy_ad_intervention/heavy_ad_service_factory.h"
#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/json_pref_store.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "media/capabilities/in_memory_video_decode_stats_db_impl.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/http/transport_security_state.h"
#include "ppapi/buildflags/buildflags.h"
#include "storage/browser/database/database_tracker.h"

#if defined(OS_ANDROID)
#include "components/prefs/scoped_user_pref_update.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_otr_delegate.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/host_zoom_map.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using content::BrowserThread;
using content::DownloadManagerDelegate;
#if !defined(OS_ANDROID)
using content::HostZoomMap;
#endif

namespace {

// Key names for OTR Profile user data.
constexpr char kVideoDecodePerfHistoryId[] = "video-decode-perf-history";

}  // namespace

OffTheRecordProfileImpl::OffTheRecordProfileImpl(
    Profile* real_profile,
    const OTRProfileID& otr_profile_id)
    : profile_(real_profile),
      otr_profile_id_(otr_profile_id),
      io_data_(this),
      start_time_(base::Time::Now()),
      key_(std::make_unique<ProfileKey>(profile_->GetPath(),
                                        profile_->GetProfileKey())) {
  prefs_ = CreateIncognitoPrefServiceSyncable(
      PrefServiceSyncableFromProfile(profile_),
      CreateExtensionPrefStore(profile_, true));

  key_->SetPrefs(prefs_.get());
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

  // Register on BrowserContext.
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

void OffTheRecordProfileImpl::Init() {
  FullBrowserTransitionManager::Get()->OnProfileCreated(this);

  // Must be done before CreateBrowserContextServices(), since some of them
  // change behavior based on whether the provided context is a guest session.
  set_is_guest_profile(profile_->IsGuestSession());

  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  // Always crash when incognito is not available.
  // Guest profiles may always be OTR, and non primary OTRs are always allowed.
  // Check IncognitoModePrefs otherwise.
  CHECK(profile_->IsGuestSession() || profile_->IsSystemProfile() ||
        !IsPrimaryOTRProfile() ||
        IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) !=
            IncognitoModePrefs::DISABLED);

#if !defined(OS_ANDROID)
  TrackZoomLevelsFromParent();
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->RegisterProfile(this);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Make the chrome//extension-icon/ resource available.
  content::URLDataSource::Add(
      this, std::make_unique<extensions::ExtensionIconSource>(profile_));

  extensions::ExtensionWebRequestEventRouter::GetInstance()
      ->OnOTRBrowserContextCreated(profile_, this);
#endif

  // The DomDistillerViewerSource is not a normal WebUI so it must be registered
  // as a URLDataSource early.
  dom_distiller::RegisterViewerSource(this);

  // AccessibilityLabelsService has a default prefs behavior in incognito.
  AccessibilityLabelsService::InitOffTheRecordPrefs(this);

  HeavyAdServiceFactory::GetForBrowserContext(this)->InitializeOffTheRecord();
}

OffTheRecordProfileImpl::~OffTheRecordProfileImpl() {
  MaybeSendDestroyedNotification();

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginServiceFilter::GetInstance()->UnregisterProfile(this);
#endif

  FullBrowserTransitionManager::Get()->OnProfileDestroyed(this);

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  SimpleKeyMap::GetInstance()->Dissociate(this);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionWebRequestEventRouter::GetInstance()
      ->OnOTRBrowserContextDestroyed(profile_, this);
#endif

  // This must be called before ProfileIOData::ShutdownOnUIThread but after
  // other profile-related destroy notifications are dispatched.
  ShutdownStoragePartitions();

  // Store incognito lifetime histogram.
  if (!IsGuestSession()) {
    auto duration = base::Time::Now() - start_time_;
    base::UmaHistogramCustomCounts(
        "Profile.Incognito.Lifetime", duration.InMinutes(), 1,
        base::TimeDelta::FromDays(28).InMinutes(), 100);
  }
}

#if !defined(OS_ANDROID)
void OffTheRecordProfileImpl::TrackZoomLevelsFromParent() {
  DCHECK(!profile_->IsIncognitoProfile());

  // Here we only want to use zoom levels stored in the main-context's default
  // storage partition. We're not interested in zoom levels in special
  // partitions, e.g. those used by WebViewGuests.
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  HostZoomMap* parent_host_zoom_map =
      HostZoomMap::GetDefaultForBrowserContext(profile_);
  host_zoom_map->CopyFrom(parent_host_zoom_map);
  // Observe parent profile's HostZoomMap changes so they can also be applied
  // to this profile's HostZoomMap.
  track_zoom_subscription_ = parent_host_zoom_map->AddZoomLevelChangedCallback(
      base::BindRepeating(&OffTheRecordProfileImpl::OnParentZoomLevelChanged,
                          base::Unretained(this)));
  if (!profile_->GetZoomLevelPrefs())
    return;

  // Also track changes to the parent profile's default zoom level.
  parent_default_zoom_level_subscription_ =
      profile_->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          base::Bind(&OffTheRecordProfileImpl::UpdateDefaultZoomLevel,
                     base::Unretained(this)));
}
#endif  // !defined(OS_ANDROID)

std::string OffTheRecordProfileImpl::GetProfileUserName() const {
  // Incognito profile should not return the username.
  return std::string();
}

Profile::ProfileType OffTheRecordProfileImpl::GetProfileType() const {
#if !defined(OS_CHROMEOS)
  return profile_->IsGuestSession() ? GUEST_PROFILE : INCOGNITO_PROFILE;
#else
  // GuestSessionProfile is used for guest sessions on ChromeOS.
  DCHECK(!profile_->IsGuestSession());
  return INCOGNITO_PROFILE;
#endif
}

base::FilePath OffTheRecordProfileImpl::GetPath() {
  return profile_->GetPath();
}

base::FilePath OffTheRecordProfileImpl::GetPath() const {
  return profile_->GetPath();
}

base::Time OffTheRecordProfileImpl::GetCreationTime() const {
  return start_time_;
}

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
OffTheRecordProfileImpl::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelOTRDelegate>(
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}
#endif  // !defined(OS_ANDROID)

scoped_refptr<base::SequencedTaskRunner>
OffTheRecordProfileImpl::GetIOTaskRunner() {
  return profile_->GetIOTaskRunner();
}

bool OffTheRecordProfileImpl::IsOffTheRecord() {
  return true;
}

bool OffTheRecordProfileImpl::IsOffTheRecord() const {
  return true;
}

const Profile::OTRProfileID& OffTheRecordProfileImpl::GetOTRProfileID() const {
  return otr_profile_id_;
}

Profile* OffTheRecordProfileImpl::GetOffTheRecordProfile(
    const OTRProfileID& otr_profile_id) {
  if (otr_profile_id_ == otr_profile_id)
    return this;
  return profile_->GetOffTheRecordProfile(otr_profile_id);
}

std::vector<Profile*> OffTheRecordProfileImpl::GetAllOffTheRecordProfiles() {
  return profile_->GetAllOffTheRecordProfiles();
}

void OffTheRecordProfileImpl::DestroyOffTheRecordProfile(
    Profile* /*otr_profile*/) {
  // OffTheRecord profiles should be destroyed through a request to their
  // original profile.
  NOTREACHED();
}

bool OffTheRecordProfileImpl::HasOffTheRecordProfile(
    const OTRProfileID& otr_profile_id) {
  if (otr_profile_id_ == otr_profile_id)
    return true;
  return profile_->HasOffTheRecordProfile(otr_profile_id);
}

bool OffTheRecordProfileImpl::HasAnyOffTheRecordProfile() {
  return true;
}

Profile* OffTheRecordProfileImpl::GetOriginalProfile() {
  return profile_;
}

const Profile* OffTheRecordProfileImpl::GetOriginalProfile() const {
  return profile_;
}

ExtensionSpecialStoragePolicy*
    OffTheRecordProfileImpl::GetExtensionSpecialStoragePolicy() {
  return GetOriginalProfile()->GetExtensionSpecialStoragePolicy();
}

bool OffTheRecordProfileImpl::IsSupervised() const {
  return profile_->IsSupervised();
}

bool OffTheRecordProfileImpl::IsChild() const {
  // TODO(treib): If we ever allow incognito for child accounts, evaluate
  // whether we want to just return false here.
  return profile_->IsChild();
}

bool OffTheRecordProfileImpl::IsLegacySupervised() const {
  return profile_->IsLegacySupervised();
}

bool OffTheRecordProfileImpl::AllowsBrowserWindows() const {
  return profile_->AllowsBrowserWindows();
}

PrefService* OffTheRecordProfileImpl::GetPrefs() {
  return prefs_.get();
}

const PrefService* OffTheRecordProfileImpl::GetPrefs() const {
  return prefs_.get();
}

PrefService* OffTheRecordProfileImpl::GetOffTheRecordPrefs() {
  return prefs_.get();
}

DownloadManagerDelegate* OffTheRecordProfileImpl::GetDownloadManagerDelegate() {
  return DownloadCoreServiceFactory::GetForBrowserContext(this)
      ->GetDownloadManagerDelegate();
}

policy::SchemaRegistryService*
OffTheRecordProfileImpl::GetPolicySchemaRegistryService() {
  return nullptr;
}

#if defined(OS_CHROMEOS)
policy::UserCloudPolicyManagerChromeOS*
OffTheRecordProfileImpl::GetUserCloudPolicyManagerChromeOS() {
  return GetOriginalProfile()->GetUserCloudPolicyManagerChromeOS();
}

policy::ActiveDirectoryPolicyManager*
OffTheRecordProfileImpl::GetActiveDirectoryPolicyManager() {
  return GetOriginalProfile()->GetActiveDirectoryPolicyManager();
}
#else
policy::UserCloudPolicyManager*
OffTheRecordProfileImpl::GetUserCloudPolicyManager() {
  return GetOriginalProfile()->GetUserCloudPolicyManager();
}
#endif  // defined(OS_CHROMEOS)

scoped_refptr<network::SharedURLLoaderFactory>
OffTheRecordProfileImpl::GetURLLoaderFactory() {
  return GetDefaultStoragePartition(this)
      ->GetURLLoaderFactoryForBrowserProcess();
}

content::ResourceContext* OffTheRecordProfileImpl::GetResourceContext() {
  return io_data_.GetResourceContext();
}

content::BrowserPluginGuestManager* OffTheRecordProfileImpl::GetGuestManager() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return NULL;
#endif
}

storage::SpecialStoragePolicy*
OffTheRecordProfileImpl::GetSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::PushMessagingService*
OffTheRecordProfileImpl::GetPushMessagingService() {
  // TODO(johnme): Support push messaging in incognito if possible.
  return NULL;
}

content::StorageNotificationService*
OffTheRecordProfileImpl::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate*
OffTheRecordProfileImpl::GetSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForProfile(this);
}

// TODO(mlamouri): we should all these BrowserContext implementation to Profile
// instead of repeating them inside all Profile implementations.
content::PermissionControllerDelegate*
OffTheRecordProfileImpl::GetPermissionControllerDelegate() {
  return PermissionManagerFactory::GetForProfile(this);
}

content::ClientHintsControllerDelegate*
OffTheRecordProfileImpl::GetClientHintsControllerDelegate() {
  return ClientHintsFactory::GetForBrowserContext(this);
}

content::BackgroundFetchDelegate*
OffTheRecordProfileImpl::GetBackgroundFetchDelegate() {
  return BackgroundFetchDelegateFactory::GetForProfile(this);
}

content::BackgroundSyncController*
OffTheRecordProfileImpl::GetBackgroundSyncController() {
  return BackgroundSyncControllerFactory::GetForProfile(this);
}

content::BrowsingDataRemoverDelegate*
OffTheRecordProfileImpl::GetBrowsingDataRemoverDelegate() {
  return ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(this);
}

media::VideoDecodePerfHistory*
OffTheRecordProfileImpl::GetVideoDecodePerfHistory() {
  media::VideoDecodePerfHistory* decode_history =
      static_cast<media::VideoDecodePerfHistory*>(
          GetUserData(kVideoDecodePerfHistoryId));

  // Lazily created. Note, this does not trigger loading the DB from disk. That
  // occurs later upon first VideoDecodePerfHistory API request that requires DB
  // access. DB operations will not block the UI thread.
  if (!decode_history) {
    // Use the original profile's DB to seed the OTR VideoDeocdePerfHisotry. The
    // original DB is treated as read-only, while OTR playbacks will write stats
    // to the InMemory version (cleared on profile destruction). Guest profiles
    // don't have a root profile like incognito, meaning they don't have a seed
    // DB to call on and we can just pass null.
    media::VideoDecodeStatsDBProvider* seed_db_provider =
        IsGuestSession() ? nullptr
                         // Safely passing raw pointer to VideoDecodePerfHistory
                         // because original profile will outlive this profile.
                         : GetOriginalProfile()->GetVideoDecodePerfHistory();

    auto stats_db = std::make_unique<media::InMemoryVideoDecodeStatsDBImpl>(
        seed_db_provider);
    // TODO(liberato): Get the FeatureProviderFactoryCB from BrowserContext.
    auto new_decode_history = std::make_unique<media::VideoDecodePerfHistory>(
        std::move(stats_db), media::learning::FeatureProviderFactoryCB());
    decode_history = new_decode_history.get();

    SetUserData(kVideoDecodePerfHistoryId, std::move(new_decode_history));
  }

  return decode_history;
}

void OffTheRecordProfileImpl::SetCorsOriginAccessListForOrigin(
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  NOTREACHED()
      << "CorsOriginAccessList should not be modified in incognito profiles";
}

content::SharedCorsOriginAccessList*
OffTheRecordProfileImpl::GetSharedCorsOriginAccessList() {
  return profile_->GetSharedCorsOriginAccessList();
}

bool OffTheRecordProfileImpl::ShouldEnableOutOfBlinkCors() {
  return profile_->ShouldEnableOutOfBlinkCors();
}

content::NativeFileSystemPermissionContext*
OffTheRecordProfileImpl::GetNativeFileSystemPermissionContext() {
  return NativeFileSystemPermissionContextFactory::GetForProfile(this);
}

bool OffTheRecordProfileImpl::IsSameProfile(Profile* profile) {
  return (profile == this) || (profile == profile_);
}

base::Time OffTheRecordProfileImpl::GetStartTime() const {
  return start_time_;
}

ProfileKey* OffTheRecordProfileImpl::GetProfileKey() const {
  DCHECK(key_);
  return key_.get();
}

policy::ProfilePolicyConnector*
OffTheRecordProfileImpl::GetProfilePolicyConnector() {
  return profile_->GetProfilePolicyConnector();
}

const policy::ProfilePolicyConnector*
OffTheRecordProfileImpl::GetProfilePolicyConnector() const {
  return profile_->GetProfilePolicyConnector();
}

void OffTheRecordProfileImpl::SetExitType(ExitType exit_type) {
}

base::FilePath OffTheRecordProfileImpl::last_selected_directory() {
  const base::FilePath& directory = last_selected_directory_;
  if (directory.empty()) {
    return profile_->last_selected_directory();
  }
  return directory;
}

void OffTheRecordProfileImpl::set_last_selected_directory(
    const base::FilePath& path) {
  last_selected_directory_ = path;
}

bool OffTheRecordProfileImpl::WasCreatedByVersionOrLater(
    const std::string& version) {
  return profile_->WasCreatedByVersionOrLater(version);
}

Profile::ExitType OffTheRecordProfileImpl::GetLastSessionExitType() {
  return profile_->GetLastSessionExitType();
}

#if defined(OS_CHROMEOS)
void OffTheRecordProfileImpl::ChangeAppLocale(const std::string& locale,
                                              AppLocaleChangedVia) {
}

void OffTheRecordProfileImpl::OnLogin() {
}

void OffTheRecordProfileImpl::InitChromeOSPreferences() {
  // The incognito profile shouldn't have Chrome OS's preferences.
  // The preferences are associated with the regular user profile.
}
#endif  // defined(OS_CHROMEOS)

GURL OffTheRecordProfileImpl::GetHomePage() {
  return profile_->GetHomePage();
}

void OffTheRecordProfileImpl::SetCreationTimeForTesting(
    base::Time creation_time) {
  start_time_ = creation_time;
}

#if defined(OS_CHROMEOS)
// Special case of the OffTheRecordProfileImpl which is used while Guest
// session in CrOS.
class GuestSessionProfile : public OffTheRecordProfileImpl {
 public:
  explicit GuestSessionProfile(Profile* real_profile)
      : OffTheRecordProfileImpl(real_profile, OTRProfileID::PrimaryID()) {
    set_is_guest_profile(true);
  }

  ProfileType GetProfileType() const override { return GUEST_PROFILE; }

  void InitChromeOSPreferences() override {
    chromeos_preferences_.reset(new chromeos::Preferences());
    chromeos_preferences_->Init(
        this, user_manager::UserManager::Get()->GetActiveUser());
  }

 private:
  // The guest user should be able to customize Chrome OS preferences.
  std::unique_ptr<chromeos::Preferences> chromeos_preferences_;
};
#endif

// static
std::unique_ptr<Profile> Profile::CreateOffTheRecordProfile(
    Profile* parent,
    const OTRProfileID& otr_profile_id) {
  std::unique_ptr<OffTheRecordProfileImpl> profile;
#if defined(OS_CHROMEOS)
  if (parent->IsGuestSession() && otr_profile_id == OTRProfileID::PrimaryID())
    profile.reset(new GuestSessionProfile(parent));
#endif
  if (!profile)
    profile.reset(new OffTheRecordProfileImpl(parent, otr_profile_id));
  profile->Init();
  return std::move(profile);
}

#if !defined(OS_ANDROID)
void OffTheRecordProfileImpl::OnParentZoomLevelChanged(
    const HostZoomMap::ZoomLevelChange& change) {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  switch (change.mode) {
    case HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM:
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_HOST:
      host_zoom_map->SetZoomLevelForHost(change.host, change.zoom_level);
      return;
    case HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST:
      host_zoom_map->SetZoomLevelForHostAndScheme(change.scheme,
          change.host,
          change.zoom_level);
      return;
    case HostZoomMap::PAGE_SCALE_IS_ONE_CHANGED:
      return;
  }
}

void OffTheRecordProfileImpl::UpdateDefaultZoomLevel() {
  HostZoomMap* host_zoom_map = HostZoomMap::GetDefaultForBrowserContext(this);
  double default_zoom_level =
      profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  host_zoom_map->SetDefaultZoomLevel(default_zoom_level);
  // HostZoomMap does not trigger zoom notification events when the default
  // zoom level is set, so we need to do it here.
  zoom::ZoomEventManager::GetForBrowserContext(this)
      ->OnDefaultZoomLevelChanged();
}
#endif  // !defined(OS_ANDROID)
