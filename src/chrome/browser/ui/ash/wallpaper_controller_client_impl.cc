// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"

#include <utility>

#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/settings/cros_settings_names.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/hash/sha1.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/customization/customization_wallpaper_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/value_store/value_store.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

using ::ash::ProfileHelper;
using extension_misc::kWallpaperManagerId;
using file_manager::VolumeManager;
using session_manager::SessionManager;
using wallpaper_handlers::BackdropSurpriseMeImageFetcher;

namespace {

// Known user keys.
const char kWallpaperFilesId[] = "wallpaper-files-id";
constexpr char kChromeAppDailyRefreshInfoPref[] = "daily-refresh-info-key";
constexpr char kChromeAppCollectionId[] = "collectionId";
constexpr char kDriveFsWallpaperDirName[] = "Chromebook Wallpaper";
// Encoded in |WallpaperControllerImpl.ResizeAndEncodeImage|.
constexpr char kDriveFsWallpaperFileName[] = "wallpaper.jpg";
constexpr char kDriveFsTempWallpaperFileName[] = "wallpaper-tmp.jpg";

WallpaperControllerClientImpl* g_wallpaper_controller_client_instance = nullptr;

bool IsKnownUser(const AccountId& account_id) {
  return user_manager::UserManager::Get()->IsKnownUser(account_id);
}

// This has once been copied from
// brillo::cryptohome::home::SanitizeUserName(username) to be used for
// wallpaper identification purpose only.
//
// Historic note: We need some way to identify users wallpaper files in
// the device filesystem. Historically User::username_hash() was used for this
// purpose, but it has two caveats:
// 1. username_hash() is defined only after user has logged in.
// 2. If cryptohome identifier changes, username_hash() will also change,
//    and we may lose user => wallpaper files mapping at that point.
// So this function gives WallpaperManager independent hashing method to break
// this dependency.
std::string HashWallpaperFilesIdStr(const std::string& files_id_unhashed) {
  chromeos::SystemSaltGetter* salt_getter = chromeos::SystemSaltGetter::Get();
  DCHECK(salt_getter);

  // System salt must be defined at this point.
  const chromeos::SystemSaltGetter::RawSalt* salt = salt_getter->GetRawSalt();
  if (!salt)
    LOG(FATAL) << "WallpaperManager HashWallpaperFilesIdStr(): no salt!";

  unsigned char binmd[base::kSHA1Length];
  std::string lowercase(files_id_unhashed);
  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 ::tolower);
  std::vector<uint8_t> data = *salt;
  std::copy(files_id_unhashed.begin(), files_id_unhashed.end(),
            std::back_inserter(data));
  base::SHA1HashBytes(data.data(), data.size(), binmd);
  std::string result = base::HexEncode(binmd, sizeof(binmd));
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return result;
}

// Returns true if wallpaper files id can be returned successfully.
bool CanGetFilesId() {
  return chromeos::SystemSaltGetter::IsInitialized() &&
         chromeos::SystemSaltGetter::Get()->GetRawSalt();
}

void GetFilesIdSaltReady(
    const AccountId& account_id,
    base::OnceCallback<void(const std::string&)> files_id_callback) {
  DCHECK(CanGetFilesId());
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (const std::string* stored_value =
          known_user.FindStringPath(account_id, kWallpaperFilesId)) {
    std::move(files_id_callback).Run(*stored_value);
    return;
  }

  const std::string wallpaper_files_id =
      HashWallpaperFilesIdStr(account_id.GetUserEmail());
  user_manager::known_user::SetStringPref(account_id, kWallpaperFilesId,
                                          wallpaper_files_id);
  std::move(files_id_callback).Run(wallpaper_files_id);
}

// Returns true if |users| contains users other than device local accounts.
bool HasNonDeviceLocalAccounts(const user_manager::UserList& users) {
  for (const user_manager::User* user : users) {
    if (!policy::IsDeviceLocalAccountUser(user->GetAccountId().GetUserEmail(),
                                          nullptr))
      return true;
  }
  return false;
}

// Returns the first public session user found in |users|, or null if there's
// none.
user_manager::User* FindPublicSession(const user_manager::UserList& users) {
  for (size_t i = 0; i < users.size(); ++i) {
    if (users[i]->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      return users[i];
  }
  return nullptr;
}

// Extract daily refresh collection id from |value_store|. If unable to fetch
// the daily refresh collection id, or the user does not have daily refresh
// configured, returns empty string. This must be run on the same sequence
// that |value_store| came from.
std::string GetDailyRefreshCollectionId(value_store::ValueStore* value_store) {
  if (!value_store)
    return std::string();

  auto read_result = value_store->Get(kChromeAppDailyRefreshInfoPref);

  if (!read_result.status().ok())
    return std::string();

  const auto* daily_refresh_info_string =
      read_result.settings().FindStringKey(kChromeAppDailyRefreshInfoPref);

  if (!daily_refresh_info_string)
    return std::string();

  const absl::optional<base::Value> daily_refresh_info =
      base::JSONReader::Read(*daily_refresh_info_string);

  if (!daily_refresh_info)
    return std::string();

  const auto* collection_id =
      daily_refresh_info->FindStringKey(kChromeAppCollectionId);

  if (!collection_id)
    return std::string();

  return *collection_id;
}

base::FilePath GetDriveFsWallpaperDir(Profile* profile) {
  CHECK(profile);

  drive::DriveIntegrationService* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!drive_integration_service) {
    return base::FilePath();
  }
  return drive_integration_service->GetMountPointPath()
      .Append(drive::util::kDriveMyDriveRootDirName)
      .Append(kDriveFsWallpaperDirName);
}

bool SaveWallpaperToDriveFsIOTaskRunner(
    const base::FilePath& origin,
    const base::FilePath& destination_directory) {
  if (destination_directory.empty())
    return false;

  if (!base::DirectoryExists(destination_directory) &&
      !base::CreateDirectory(destination_directory)) {
    return false;
  }

  std::string temp_file_name =
      base::UnguessableToken::Create().ToString().append(
          kDriveFsTempWallpaperFileName);
  base::FilePath temp_destination =
      destination_directory.Append(temp_file_name);
  if (!base::CopyFile(origin, temp_destination)) {
    base::DeleteFile(temp_destination);
    return false;
  }

  base::FilePath destination =
      destination_directory.Append(kDriveFsWallpaperFileName);
  bool success = base::ReplaceFile(temp_destination, destination, nullptr);
  base::DeleteFile(temp_destination);
  return success;
}

}  // namespace

WallpaperControllerClientImpl::WallpaperControllerClientImpl() {
  local_state_ = g_browser_process->local_state();
  show_user_names_on_signin_subscription_ =
      ash::CrosSettings::Get()->AddSettingsObserver(
          ash::kAccountsPrefShowUserNamesOnSignIn,
          base::BindRepeating(
              &WallpaperControllerClientImpl::ShowWallpaperOnLoginScreen,
              weak_factory_.GetWeakPtr()));

  DCHECK(!g_wallpaper_controller_client_instance);
  g_wallpaper_controller_client_instance = this;

  SessionManager* session_manager = SessionManager::Get();
  // SessionManager might not exist in unit tests.
  if (session_manager)
    session_observation_.Observe(session_manager);

  io_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

WallpaperControllerClientImpl::~WallpaperControllerClientImpl() {
  wallpaper_controller_->SetClient(nullptr);
  weak_factory_.InvalidateWeakPtrs();
  DCHECK_EQ(this, g_wallpaper_controller_client_instance);
  g_wallpaper_controller_client_instance = nullptr;
}

void WallpaperControllerClientImpl::Init() {
  pref_registrar_.Init(local_state_);
  pref_registrar_.Add(
      prefs::kDeviceWallpaperImageFilePath,
      base::BindRepeating(
          &WallpaperControllerClientImpl::DeviceWallpaperImageFilePathChanged,
          weak_factory_.GetWeakPtr()));
  wallpaper_controller_ = ash::WallpaperController::Get();

  InitController();
}

void WallpaperControllerClientImpl::InitForTesting(
    ash::WallpaperController* controller) {
  wallpaper_controller_ = controller;
  InitController();
}

void WallpaperControllerClientImpl::SetInitialWallpaper() {
  // Apply device customization.
  namespace customization_util = ash::customization_wallpaper_util;
  if (customization_util::ShouldUseCustomizedDefaultWallpaper()) {
    base::FilePath customized_default_small_path;
    base::FilePath customized_default_large_path;
    if (customization_util::GetCustomizedDefaultWallpaperPaths(
            &customized_default_small_path, &customized_default_large_path)) {
      wallpaper_controller_->SetCustomizedDefaultWallpaperPaths(
          customized_default_small_path, customized_default_large_path);
    }
  }

  // Guest wallpaper should be initialized when guest logs in.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kGuestSession)) {
    return;
  }

  // Do not set wallpaper in tests.
  if (ash::WizardController::IsZeroDelayEnabled())
    return;

  // Show the wallpaper of the active user during an user session.
  if (user_manager::UserManager::Get()->IsUserLoggedIn()) {
    ShowUserWallpaper(
        user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
    return;
  }

  // Show a white wallpaper during OOBE.
  if (SessionManager::Get()->session_state() ==
      session_manager::SessionState::OOBE) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    bitmap.eraseColor(SK_ColorWHITE);
    wallpaper_controller_->ShowOneShotWallpaper(
        gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    return;
  }

  ShowWallpaperOnLoginScreen();
}

// static
WallpaperControllerClientImpl* WallpaperControllerClientImpl::Get() {
  return g_wallpaper_controller_client_instance;
}

void WallpaperControllerClientImpl::SetCustomWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image,
    bool preview_mode) {
  if (!IsKnownUser(account_id))
    return;
  wallpaper_controller_->SetCustomWallpaper(account_id, file_name, layout,
                                            image, preview_mode);
}

void WallpaperControllerClientImpl::SetOnlineWallpaper(
    const ash::OnlineWallpaperParams& params,
    ash::WallpaperController::SetOnlineWallpaperCallback callback) {
  if (!IsKnownUser(params.account_id))
    return;

  wallpaper_controller_->SetOnlineWallpaper(params, std::move(callback));
}

void WallpaperControllerClientImpl::SetGooglePhotosWallpaper(
    const ash::GooglePhotosWallpaperParams& params,
    ash::WallpaperController::SetGooglePhotosWallpaperCallback callback) {
  if (!IsKnownUser(params.account_id))
    return;

  wallpaper_controller_->SetGooglePhotosWallpaper(params, std::move(callback));
}

void WallpaperControllerClientImpl::SetOnlineWallpaperIfExists(
    const ash::OnlineWallpaperParams& params,
    ash::WallpaperController::SetOnlineWallpaperCallback callback) {
  if (!IsKnownUser(params.account_id))
    return;
  wallpaper_controller_->SetOnlineWallpaperIfExists(params,
                                                    std::move(callback));
}

void WallpaperControllerClientImpl::SetOnlineWallpaperFromData(
    const ash::OnlineWallpaperParams& params,
    const std::string& image_data,
    ash::WallpaperController::SetOnlineWallpaperCallback callback) {
  if (!IsKnownUser(params.account_id))
    return;
  wallpaper_controller_->SetOnlineWallpaperFromData(params, image_data,
                                                    std::move(callback));
}

void WallpaperControllerClientImpl::SetCustomizedDefaultWallpaperPaths(
    const base::FilePath& customized_default_small_path,
    const base::FilePath& customized_default_large_path) {
  wallpaper_controller_->SetCustomizedDefaultWallpaperPaths(
      customized_default_small_path, customized_default_large_path);
}

void WallpaperControllerClientImpl::SetPolicyWallpaper(
    const AccountId& account_id,
    std::unique_ptr<std::string> data) {
  if (!data || !IsKnownUser(account_id))
    return;

  wallpaper_controller_->SetPolicyWallpaper(account_id, *data);
}

bool WallpaperControllerClientImpl::SetThirdPartyWallpaper(
    const AccountId& account_id,
    const std::string& file_name,
    ash::WallpaperLayout layout,
    const gfx::ImageSkia& image) {
  return IsKnownUser(account_id) &&
         wallpaper_controller_->SetThirdPartyWallpaper(account_id, file_name,
                                                       layout, image);
}

void WallpaperControllerClientImpl::ConfirmPreviewWallpaper() {
  wallpaper_controller_->ConfirmPreviewWallpaper();
}

void WallpaperControllerClientImpl::CancelPreviewWallpaper() {
  wallpaper_controller_->CancelPreviewWallpaper();
}

void WallpaperControllerClientImpl::UpdateCustomWallpaperLayout(
    const AccountId& account_id,
    ash::WallpaperLayout layout) {
  if (IsKnownUser(account_id))
    wallpaper_controller_->UpdateCustomWallpaperLayout(account_id, layout);
}

void WallpaperControllerClientImpl::ShowUserWallpaper(
    const AccountId& account_id) {
  if (IsKnownUser(account_id))
    wallpaper_controller_->ShowUserWallpaper(account_id);
}

void WallpaperControllerClientImpl::ShowSigninWallpaper() {
  wallpaper_controller_->ShowSigninWallpaper();
}

void WallpaperControllerClientImpl::ShowAlwaysOnTopWallpaper(
    const base::FilePath& image_path) {
  wallpaper_controller_->ShowAlwaysOnTopWallpaper(image_path);
}

void WallpaperControllerClientImpl::RemoveAlwaysOnTopWallpaper() {
  wallpaper_controller_->RemoveAlwaysOnTopWallpaper();
}

void WallpaperControllerClientImpl::RemoveUserWallpaper(
    const AccountId& account_id) {
  if (!IsKnownUser(account_id))
    return;

  wallpaper_controller_->RemoveUserWallpaper(account_id);
}

void WallpaperControllerClientImpl::RemovePolicyWallpaper(
    const AccountId& account_id) {
  if (!IsKnownUser(account_id))
    return;

  wallpaper_controller_->RemovePolicyWallpaper(account_id);
}

void WallpaperControllerClientImpl::GetOfflineWallpaperList(
    ash::WallpaperController::GetOfflineWallpaperListCallback callback) {
  wallpaper_controller_->GetOfflineWallpaperList(std::move(callback));
}

void WallpaperControllerClientImpl::SetAnimationDuration(
    const base::TimeDelta& animation_duration) {
  wallpaper_controller_->SetAnimationDuration(animation_duration);
}

void WallpaperControllerClientImpl::OpenWallpaperPickerIfAllowed() {
  wallpaper_controller_->OpenWallpaperPickerIfAllowed();
}

void WallpaperControllerClientImpl::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  wallpaper_controller_->MinimizeInactiveWindows(user_id_hash);
}

void WallpaperControllerClientImpl::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  wallpaper_controller_->RestoreMinimizedWindows(user_id_hash);
}

void WallpaperControllerClientImpl::AddObserver(
    ash::WallpaperControllerObserver* observer) {
  wallpaper_controller_->AddObserver(observer);
}

void WallpaperControllerClientImpl::RemoveObserver(
    ash::WallpaperControllerObserver* observer) {
  wallpaper_controller_->RemoveObserver(observer);
}

gfx::ImageSkia WallpaperControllerClientImpl::GetWallpaperImage() {
  return wallpaper_controller_->GetWallpaperImage();
}

const std::vector<SkColor>&
WallpaperControllerClientImpl::GetWallpaperColors() {
  return wallpaper_controller_->GetWallpaperColors();
}

bool WallpaperControllerClientImpl::IsWallpaperBlurred() {
  return wallpaper_controller_->IsWallpaperBlurredForLockState();
}

bool WallpaperControllerClientImpl::IsActiveUserWallpaperControlledByPolicy() {
  return wallpaper_controller_->IsActiveUserWallpaperControlledByPolicy();
}

ash::WallpaperInfo WallpaperControllerClientImpl::GetActiveUserWallpaperInfo() {
  return wallpaper_controller_->GetActiveUserWallpaperInfo();
}

bool WallpaperControllerClientImpl::ShouldShowWallpaperSetting() {
  return wallpaper_controller_->ShouldShowWallpaperSetting();
}

void WallpaperControllerClientImpl::SaveWallpaperToDriveFs(
    const AccountId& account_id,
    const base::FilePath& origin,
    base::OnceCallback<void(bool)> wallpaper_saved_callback) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  base::FilePath destination_directory = GetDriveFsWallpaperDir(profile);
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveWallpaperToDriveFsIOTaskRunner, origin,
                     destination_directory),
      std::move(wallpaper_saved_callback));
}

base::FilePath WallpaperControllerClientImpl::GetWallpaperPathFromDriveFs(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  base::FilePath wallpaper_directory = GetDriveFsWallpaperDir(profile);
  if (wallpaper_directory.empty())
    return wallpaper_directory;
  return wallpaper_directory.Append(kDriveFsWallpaperFileName);
}

void WallpaperControllerClientImpl::GetFilesId(
    const AccountId& account_id,
    base::OnceCallback<void(const std::string&)> files_id_callback) const {
  chromeos::SystemSaltGetter::Get()->AddOnSystemSaltReady(base::BindOnce(
      &GetFilesIdSaltReady, account_id, std::move(files_id_callback)));
}

bool WallpaperControllerClientImpl::IsWallpaperSyncEnabled(
    const AccountId& account_id) const {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!profile)
    return false;

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return false;
  if (chromeos::features::IsSyncSettingsCategorizationEnabled()) {
    // If in client use profile otherwise use GetUserPrefServiceSyncable.
    return profile->GetPrefs()->GetBoolean(
        chromeos::settings::prefs::kSyncOsWallpaper);
  }
  return sync_service->CanSyncFeatureStart() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kThemes);
}

void WallpaperControllerClientImpl::OnVolumeMounted(
    chromeos::MountError error_code,
    const file_manager::Volume& volume) {
  if (error_code != chromeos::MOUNT_ERROR_NONE) {
    return;
  }
  if (volume.type() != file_manager::VolumeType::VOLUME_TYPE_GOOGLE_DRIVE) {
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  VolumeManager* volume_manager = VolumeManager::Get(profile);
  // Volume ID is based on the mount path, which for drive is based on the
  // account_id.
  if (!volume_manager->FindVolumeById(volume.volume_id())) {
    return;
  }
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  CHECK(user);
  wallpaper_controller_->SyncLocalAndRemotePrefs(user->GetAccountId());
}

void WallpaperControllerClientImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  wallpaper_controller_->SyncLocalAndRemotePrefs(account_id);
  ObserveVolumeManagerForAccountId(account_id);
}

void WallpaperControllerClientImpl::MigrateCollectionIdFromValueStoreForTesting(
    const AccountId& account_id,
    value_store::ValueStore* value_store) {
  SetDailyRefreshCollectionId(account_id,
                              GetDailyRefreshCollectionId(value_store));
}

void WallpaperControllerClientImpl::DeviceWallpaperImageFilePathChanged() {
  wallpaper_controller_->SetDevicePolicyWallpaperPath(
      GetDeviceWallpaperImageFilePath());
}

void WallpaperControllerClientImpl::InitController() {
  wallpaper_controller_->SetClient(this);

  base::FilePath user_data;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data));
  base::FilePath wallpapers;
  CHECK(base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS, &wallpapers));
  base::FilePath custom_wallpapers;
  CHECK(base::PathService::Get(chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS,
                               &custom_wallpapers));
  base::FilePath device_policy_wallpaper = GetDeviceWallpaperImageFilePath();
  wallpaper_controller_->Init(user_data, wallpapers, custom_wallpapers,
                              device_policy_wallpaper);
}

void WallpaperControllerClientImpl::ShowWallpaperOnLoginScreen() {
  if (user_manager::UserManager::Get()->IsUserLoggedIn())
    return;

  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  user_manager::User* public_session = FindPublicSession(users);

  // Show the default signin wallpaper if there's no user to display.
  if ((!ShouldShowUserNamesOnLogin() && !public_session) ||
      !HasNonDeviceLocalAccounts(users)) {
    ShowSigninWallpaper();
    return;
  }

  // Normal boot, load user wallpaper.
  const AccountId account_id = public_session ? public_session->GetAccountId()
                                              : users[0]->GetAccountId();
  ShowUserWallpaper(account_id);
}

void WallpaperControllerClientImpl::OpenWallpaperPicker() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  if (ash::features::IsWallpaperWebUIEnabled()) {
    web_app::SystemAppLaunchParams params;
    params.url = GURL(ash::kChromeUIPersonalizationAppWallpaperSubpageURL);
    params.launch_source = apps::mojom::LaunchSource::kFromShelf;
    web_app::LaunchSystemWebAppAsync(
        profile, web_app::SystemAppType::PERSONALIZATION, params);
    return;
  }

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (proxy->AppRegistryCache().GetAppType(kWallpaperManagerId) ==
      apps::mojom::AppType::kUnknown) {
    return;
  }

  proxy->Launch(
      kWallpaperManagerId,
      apps::GetEventFlags(apps::mojom::LaunchContainer::kLaunchContainerWindow,
                          WindowOpenDisposition::NEW_WINDOW,
                          false /* preferred_containner */),
      apps::mojom::LaunchSource::kFromShelf);
}

void WallpaperControllerClientImpl::MaybeClosePreviewWallpaper() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::WALLPAPER_PRIVATE_ON_CLOSE_PREVIEW_WALLPAPER,
      extensions::api::wallpaper_private::OnClosePreviewWallpaper::kEventName,
      std::vector<base::Value>());
  event_router->DispatchEventToExtension(kWallpaperManagerId, std::move(event));
}

void WallpaperControllerClientImpl::SetDefaultWallpaper(
    const AccountId& account_id,
    bool show_wallpaper) {
  if (!IsKnownUser(account_id))
    return;

  wallpaper_controller_->SetDefaultWallpaper(account_id, show_wallpaper);
}

void WallpaperControllerClientImpl::MigrateCollectionIdFromChromeApp(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  auto* extension_registry = extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension =
      extension_registry->GetInstalledExtension(kWallpaperManagerId);

  // Although not now, there will be a day where this application no longer
  // exists.
  if (!extension) {
    SetDailyRefreshCollectionId(account_id, std::string());
    return;
  }

  // Get a ptr to current sequence.
  const scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunnerHandle::Get();

  auto* storage_frontend = extensions::StorageFrontend::Get(profile);
  // Callback runs on a backend sequence.
  storage_frontend->RunWithStorage(
      extension, extensions::settings_namespace::LOCAL,
      base::BindOnce(
          &WallpaperControllerClientImpl::OnGetWallpaperChromeAppValueStore,
          storage_weak_factory_.GetWeakPtr(), task_runner, account_id));
}

void WallpaperControllerClientImpl::FetchDailyRefreshWallpaper(
    const std::string& collection_id,
    DailyWallpaperUrlFetchedCallback callback) {
  surprise_me_image_fetcher_ = std::make_unique<BackdropSurpriseMeImageFetcher>(
      collection_id, /*resume_token=*/std::string());
  surprise_me_image_fetcher_->Start(
      base::BindOnce(&WallpaperControllerClientImpl::OnDailyImageInfoFetched,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WallpaperControllerClientImpl::FetchImagesForCollection(
    const std::string& collection_id,
    FetchImagesForCollectionCallback callback) {
  auto images_info_fetcher =
      std::make_unique<wallpaper_handlers::BackdropImageInfoFetcher>(
          collection_id);
  auto* images_info_fetcher_ptr = images_info_fetcher.get();
  images_info_fetcher_ptr->Start(
      base::BindOnce(&WallpaperControllerClientImpl::OnFetchImagesForCollection,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(images_info_fetcher)));
}

bool WallpaperControllerClientImpl::ShouldShowUserNamesOnLogin() const {
  bool show_user_names = true;
  ash::CrosSettings::Get()->GetBoolean(ash::kAccountsPrefShowUserNamesOnSignIn,
                                       &show_user_names);
  return show_user_names;
}

base::FilePath
WallpaperControllerClientImpl::GetDeviceWallpaperImageFilePath() {
  return base::FilePath(
      local_state_->GetString(prefs::kDeviceWallpaperImageFilePath));
}

void WallpaperControllerClientImpl::OnGetWallpaperChromeAppValueStore(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    const AccountId& account_id,
    value_store::ValueStore* value_store) {
  DCHECK(extensions::IsOnBackendSequence());
  std::string collection_id = GetDailyRefreshCollectionId(value_store);
  // Jump back to original task runner.
  main_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WallpaperControllerClientImpl::SetDailyRefreshCollectionId,
          weak_factory_.GetWeakPtr(), account_id, collection_id));
}

void WallpaperControllerClientImpl::SetDailyRefreshCollectionId(
    const AccountId& account_id,
    const std::string& collection_id) {
  wallpaper_controller_->SetDailyRefreshCollectionId(account_id, collection_id);
}

void WallpaperControllerClientImpl::OnDailyImageInfoFetched(
    DailyWallpaperUrlFetchedCallback callback,
    bool success,
    const backdrop::Image& image,
    const std::string& next_resume_token) {
  std::move(callback).Run(success, std::move(image));
  surprise_me_image_fetcher_.reset();
}

void WallpaperControllerClientImpl::OnFetchImagesForCollection(
    FetchImagesForCollectionCallback callback,
    std::unique_ptr<wallpaper_handlers::BackdropImageInfoFetcher> fetcher,
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  std::move(callback).Run(success, std::move(images));
}

void WallpaperControllerClientImpl::ObserveVolumeManagerForAccountId(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  CHECK(profile);
  volume_manager_observation_.AddObservation(VolumeManager::Get(profile));
}

void WallpaperControllerClientImpl::RecordWallpaperSourceUMA(
    const ash::WallpaperType type) {
  base::UmaHistogramEnumeration("Ash.Wallpaper.Source2", type,
                                ash::WallpaperType::kCount);
}
