// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/wallpaper/wallpaper_enumerator.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_handlers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/webui/settings/chromeos/pref_names.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/event_router.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/app_locale_settings.h"
#include "url/gurl.h"

using base::Value;
namespace wallpaper_base = extensions::api::wallpaper;
namespace wallpaper_private = extensions::api::wallpaper_private;
namespace set_wallpaper_if_exists = wallpaper_private::SetWallpaperIfExists;
namespace set_wallpaper = wallpaper_private::SetWallpaper;
namespace set_custom_wallpaper = wallpaper_private::SetCustomWallpaper;
namespace set_custom_wallpaper_layout =
    wallpaper_private::SetCustomWallpaperLayout;
namespace get_thumbnail = wallpaper_private::GetThumbnail;
namespace save_thumbnail = wallpaper_private::SaveThumbnail;
namespace record_wallpaper_uma = wallpaper_private::RecordWallpaperUMA;
namespace get_collections_info = wallpaper_private::GetCollectionsInfo;
namespace get_images_info = wallpaper_private::GetImagesInfo;
namespace get_local_image_paths = wallpaper_private::GetLocalImagePaths;
namespace get_local_image_data = wallpaper_private::GetLocalImageData;
namespace get_current_wallpaper_thumbnail =
    wallpaper_private::GetCurrentWallpaperThumbnail;
namespace get_surprise_me_image = wallpaper_private::GetSurpriseMeImage;

namespace {

// The time and retry limit to re-check the profile sync service status. The
// sync extension function can get the correct value of the "syncThemes" user
// preference only after the profile sync service has been configured.
constexpr base::TimeDelta kRetryDelay = base::Seconds(10);
constexpr int kRetryLimit = 3;

// TODO(https://crbug.com/1037624): Update "themes" terminology after sync-split
// ships.
constexpr char kSyncThemes[] = "syncThemes";

bool IsOEMDefaultWallpaper() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDefaultWallpaperIsOem);
}

// Saves |data| as |file_name| to directory with |key|. Return false if the
// directory can not be found/created or failed to write file.
bool SaveData(int key,
              const std::string& file_name,
              const std::vector<uint8_t>& data) {
  base::FilePath data_dir;
  CHECK(base::PathService::Get(key, &data_dir));
  if (!base::DirectoryExists(data_dir) &&
      !base::CreateDirectory(data_dir)) {
    return false;
  }
  base::FilePath file_path = data_dir.Append(file_name);

  return base::PathExists(file_path) ||
         base::WriteFile(file_path, reinterpret_cast<const char*>(data.data()),
                         data.size()) != -1;
}

// Gets |file_name| from directory with |key|. Return false if the directory can
// not be found or failed to read file to string |data|. Note if the |file_name|
// can not be found in the directory, return true with empty |data|. It is
// expected that we may try to access file which did not saved yet.
bool GetData(const base::FilePath& path, std::string* data) {
  base::FilePath data_dir = path.DirName();
  if (!base::DirectoryExists(data_dir) &&
      !base::CreateDirectory(data_dir))
    return false;

  return !base::PathExists(path) ||
         base::ReadFileToString(path, data);
}

// Gets the |User| for a given |BrowserContext|. The function will only return
// valid objects.
const user_manager::User* GetUserFromBrowserContext(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

ash::WallpaperType GetWallpaperType(wallpaper_private::WallpaperSource source) {
  switch (source) {
    case wallpaper_private::WALLPAPER_SOURCE_ONLINE:
      return ash::WallpaperType::kOnline;
    case wallpaper_private::WALLPAPER_SOURCE_DAILY:
      return ash::WallpaperType::kDaily;
    case wallpaper_private::WALLPAPER_SOURCE_CUSTOM:
      return ash::WallpaperType::kCustomized;
    case wallpaper_private::WALLPAPER_SOURCE_OEM:
      return ash::WallpaperType::kDefault;
    case wallpaper_private::WALLPAPER_SOURCE_THIRDPARTY:
      return ash::WallpaperType::kThirdParty;
    default:
      return ash::WallpaperType::kOnline;
  }
}

// Helper function to parse the data from a |backdrop::Image| object and save it
// to |image_info_out|.
void ParseImageInfo(
    const backdrop::Image& image,
    extensions::api::wallpaper_private::ImageInfo* image_info_out) {
  // The info of each image should contain image asset id, image url, action url
  // and display text.
  image_info_out->asset_id = base::NumberToString(image.asset_id());
  image_info_out->image_url = image.image_url();
  image_info_out->action_url = image.action_url();
  // Display text may have more than one strings.
  for (int i = 0; i < image.attribution_size(); ++i)
    image_info_out->display_text.push_back(image.attribution()[i].text());
}

}  // namespace

ExtensionFunction::ResponseAction WallpaperPrivateGetStringsFunction::Run() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("webFontFamily", IDS_WEB_FONT_FAMILY);
  SET_STRING("webFontSize", IDS_WEB_FONT_SIZE);
  SET_STRING("wallpaperAppName", IDS_WALLPAPER_MANAGER_APP_NAME);
  SET_STRING("allCategoryLabel", IDS_WALLPAPER_MANAGER_ALL_CATEGORY_LABEL);
  SET_STRING("deleteCommandLabel", IDS_WALLPAPER_MANAGER_DELETE_COMMAND_LABEL);
  SET_STRING("customCategoryLabel",
             IDS_WALLPAPER_MANAGER_MY_IMAGES_CATEGORY_LABEL);
  SET_STRING("selectCustomLabel",
             IDS_WALLPAPER_MANAGER_SELECT_CUSTOM_LABEL);
  SET_STRING("positionLabel", IDS_WALLPAPER_MANAGER_POSITION_LABEL);
  SET_STRING("colorLabel", IDS_WALLPAPER_MANAGER_COLOR_LABEL);
  SET_STRING("refreshLabel", IDS_WALLPAPER_MANAGER_REFRESH_LABEL);
  SET_STRING("exploreLabel", IDS_WALLPAPER_MANAGER_EXPLORE_LABEL);
  SET_STRING("centerCroppedLayout",
             IDS_WALLPAPER_MANAGER_LAYOUT_CENTER_CROPPED);
  SET_STRING("centerLayout", IDS_WALLPAPER_MANAGER_LAYOUT_CENTER);
  SET_STRING("stretchLayout", IDS_WALLPAPER_MANAGER_LAYOUT_STRETCH);
  SET_STRING("connectionFailed", IDS_WALLPAPER_MANAGER_NETWORK_ERROR);
  SET_STRING("downloadFailed", IDS_WALLPAPER_MANAGER_IMAGE_ERROR);
  SET_STRING("downloadCanceled", IDS_WALLPAPER_MANAGER_DOWNLOAD_CANCEL);
  SET_STRING("customWallpaperWarning",
             IDS_WALLPAPER_MANAGER_SHOW_CUSTOM_WALLPAPER_ON_START_WARNING);
  SET_STRING("accessFileFailure", IDS_WALLPAPER_MANAGER_ACCESS_FILE_FAILURE);
  SET_STRING("invalidWallpaper", IDS_WALLPAPER_MANAGER_INVALID_WALLPAPER);
  SET_STRING("noImagesAvailable", IDS_WALLPAPER_MANAGER_NO_IMAGES_AVAILABLE);
  SET_STRING("surpriseMeLabel", IDS_WALLPAPER_MANAGER_DAILY_REFRESH_LABEL);
  SET_STRING("learnMore", IDS_LEARN_MORE);
  SET_STRING("currentWallpaperSetByMessage",
             IDS_CURRENT_WALLPAPER_SET_BY_MESSAGE);
  SET_STRING("currentlySetLabel", IDS_WALLPAPER_MANAGER_CURRENTLY_SET_LABEL);
  SET_STRING("confirmPreviewLabel",
             IDS_WALLPAPER_MANAGER_CONFIRM_PREVIEW_WALLPAPER_LABEL);
  SET_STRING("setSuccessfullyMessage",
             IDS_WALLPAPER_MANAGER_SET_SUCCESSFULLY_MESSAGE);
  SET_STRING("defaultWallpaperLabel", IDS_DEFAULT_WALLPAPER_ACCESSIBLE_LABEL);
  SET_STRING("backButton", IDS_ACCNAME_BACK);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  dict->SetBoolean("isOEMDefaultWallpaper", IsOEMDefaultWallpaper());
  dict->SetString("canceledWallpaper",
                  wallpaper_api_util::kCancelWallpaperMessage);
  dict->SetString(
      "highResolutionSuffix",
      ash::WallpaperController::Get()->GetBackdropWallpaperSuffix());

  auto info =
      WallpaperControllerClientImpl::Get()->GetActiveUserWallpaperInfo();
  dict->SetString("currentWallpaper", info.location);
  dict->SetString("currentWallpaperLayout",
                  wallpaper_api_util::GetLayoutString(info.layout));

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
}

ExtensionFunction::ResponseAction
WallpaperPrivateGetSyncSettingFunction::Run() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WallpaperPrivateGetSyncSettingFunction::CheckSyncServiceStatus,
          this));
  return RespondLater();
}

void WallpaperPrivateGetSyncSettingFunction::CheckSyncServiceStatus() {
  auto dict = std::make_unique<base::DictionaryValue>();

  if (retry_number_ > kRetryLimit) {
    // It's most likely that the wallpaper synchronization is enabled (It's
    // enabled by default so unless the user disables it explicitly it remains
    // enabled).
    dict->SetBoolean(kSyncThemes, true);
    Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
    return;
  }

  Profile* profile =  Profile::FromBrowserContext(browser_context());
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    // Sync flag is disabled (perhaps prohibited by policy).
    dict->SetBoolean(kSyncThemes, false);
    Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
    return;
  }

  if (chromeos::features::IsSyncSettingsCategorizationEnabled()) {
    // When the sync settings categorization is on, the wallpaper sync status is
    // stored in the kSyncOsWallpaper pref. The pref value essentially means
    // "themes sync is on" && "apps sync is on".
    // TODO(https://crbug.com/1243218): Figure out if we need to check
    // IsOsSyncFeatureEnabled here.
    bool os_wallpaper_sync_enabled =
        sync_service->GetUserSettings()->IsOsSyncFeatureEnabled() &&
        profile->GetPrefs()->GetBoolean(
            chromeos::settings::prefs::kSyncOsWallpaper);
    dict->SetBoolean(kSyncThemes, os_wallpaper_sync_enabled);
    Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
    return;
  }

  if (!sync_service->CanSyncFeatureStart()) {
    // Sync-the-feature is disabled.
    dict->SetBoolean(kSyncThemes, false);
    Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
    return;
  }

  if (sync_service->GetUserSettings()->IsFirstSetupComplete()) {
    // When sync settings categorization is disabled, wallpaper is synced as a
    // group with browser themes.
    dict->SetBoolean(kSyncThemes,
                     sync_service->GetUserSettings()->GetSelectedTypes().Has(
                         syncer::UserSelectableType::kThemes));
    Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(dict))));
    return;
  }

  // The user hasn't finished setting up sync, so we don't know whether they'll
  // want to sync themes. Try again in a bit.
  // TODO(https://crbug.com/1036448): It would be cleaner to implement a
  // SyncServiceObserver and wait for OnStateChanged() instead of polling.
  retry_number_++;
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &WallpaperPrivateGetSyncSettingFunction::CheckSyncServiceStatus,
          this),
      retry_number_ * kRetryDelay);
}

WallpaperPrivateSetWallpaperIfExistsFunction::
    WallpaperPrivateSetWallpaperIfExistsFunction() {}

WallpaperPrivateSetWallpaperIfExistsFunction::
    ~WallpaperPrivateSetWallpaperIfExistsFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateSetWallpaperIfExistsFunction::Run() {
  std::unique_ptr<
      extensions::api::wallpaper_private::SetWallpaperIfExists::Params>
      params = set_wallpaper_if_exists::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert |asset_id| from string to optional uint64_t. Empty string results
  // in nullopt. |from_user| is true when the request is from a user's
  // action i.e. |asset_id| is not nullopt.
  absl::optional<uint64_t> asset_id;
  bool from_user = false;
  if (!params->asset_id.empty()) {
    uint64_t value = 0;
    if (!base::StringToUint64(params->asset_id, &value))
      return RespondNow(Error("Failed to parse asset_id."));
    asset_id = value;
    from_user = true;
  }

  WallpaperControllerClientImpl::Get()->SetOnlineWallpaperIfExists(
      ash::OnlineWallpaperParams(
          GetUserFromBrowserContext(browser_context())->GetAccountId(),
          asset_id, GURL(params->url), params->collection_id,
          wallpaper_api_util::GetLayoutEnum(
              wallpaper_base::ToString(params->layout)),
          params->preview_mode, from_user, /*daily_refresh_enabled=*/false,
          /*unit_id=*/absl::nullopt,
          /*variants=*/std::vector<ash::OnlineWallpaperVariant>()),
      base::BindOnce(&WallpaperPrivateSetWallpaperIfExistsFunction::
                         OnSetOnlineWallpaperIfExistsCallback,
                     this));
  return RespondLater();
}

void WallpaperPrivateSetWallpaperIfExistsFunction::
    OnSetOnlineWallpaperIfExistsCallback(bool file_exists) {
  if (file_exists) {
    Respond(OneArgument(base::Value(true)));
  } else {
    auto args = std::make_unique<base::ListValue>();
    // TODO(crbug.com/830212): Do not send arguments when the function fails.
    // Call sites should inspect chrome.runtime.lastError instead.
    args->Append(false);
    Respond(ErrorWithArguments(
        std::move(args), "The wallpaper doesn't exist in local file system."));
  }
}

WallpaperPrivateSetWallpaperFunction::WallpaperPrivateSetWallpaperFunction() {
}

WallpaperPrivateSetWallpaperFunction::~WallpaperPrivateSetWallpaperFunction() {
}

ExtensionFunction::ResponseAction WallpaperPrivateSetWallpaperFunction::Run() {
  std::unique_ptr<extensions::api::wallpaper_private::SetWallpaper::Params>
      params = set_wallpaper::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // |from_user| is false as this method is called after
  // |WallpaperPrivateSetWallpaperIfExistsFunction| which has already handled
  // extra logic tied with |from_user|.
  WallpaperControllerClientImpl::Get()->SetOnlineWallpaperFromData(
      ash::OnlineWallpaperParams(
          GetUserFromBrowserContext(browser_context())->GetAccountId(),
          /*asset_id=*/absl::nullopt, GURL(params->url),
          /*collection_id=*/std::string(),
          wallpaper_api_util::GetLayoutEnum(
              wallpaper_base::ToString(params->layout)),
          params->preview_mode, /*from_user=*/false,
          /*daily_refresh_enabled=*/false, /*unit_id=*/absl::nullopt,
          /*variants=*/std::vector<ash::OnlineWallpaperVariant>()),
      std::string(params->wallpaper.begin(), params->wallpaper.end()),
      base::BindOnce(
          &WallpaperPrivateSetWallpaperFunction::OnSetWallpaperCallback, this));
  return RespondLater();
}

void WallpaperPrivateSetWallpaperFunction::OnSetWallpaperCallback(
    bool success) {
  if (!success) {
    Respond(Error("Failed to set wallpaper."));
    return;
  }

  Respond(NoArguments());
}

WallpaperPrivateResetWallpaperFunction::
    WallpaperPrivateResetWallpaperFunction() {}

WallpaperPrivateResetWallpaperFunction::
    ~WallpaperPrivateResetWallpaperFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateResetWallpaperFunction::Run() {
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();

  WallpaperControllerClientImpl::Get()->SetDefaultWallpaper(
      account_id, true /* show_wallpaper */);
  return RespondNow(NoArguments());
}

WallpaperPrivateSetCustomWallpaperFunction::
    WallpaperPrivateSetCustomWallpaperFunction() {}

WallpaperPrivateSetCustomWallpaperFunction::
    ~WallpaperPrivateSetCustomWallpaperFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateSetCustomWallpaperFunction::Run() {
  params = set_custom_wallpaper::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Gets account id from the caller, ensuring multiprofile compatibility.
  const user_manager::User* user = GetUserFromBrowserContext(browser_context());
  account_id_ = user->GetAccountId();

  StartDecode(params->wallpaper);

  return RespondLater();
}

void WallpaperPrivateSetCustomWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  ash::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(layout);

  const std::string file_name =
      base::FilePath(params->file_name).BaseName().value();
  WallpaperControllerClientImpl::Get()->SetCustomWallpaper(
      account_id_, file_name, layout, image, params->preview_mode);
  unsafe_wallpaper_decoder_ = nullptr;

  if (params->generate_thumbnail) {
    image.EnsureRepsForSupportedScales();
    std::vector<uint8_t> thumbnail_data = GenerateThumbnail(
        image, gfx::Size(kWallpaperThumbnailWidth, kWallpaperThumbnailHeight));
    Respond(OneArgument(base::Value(std::move(thumbnail_data))));
  } else {
    Respond(NoArguments());
  }
}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

WallpaperPrivateSetCustomWallpaperLayoutFunction::
    ~WallpaperPrivateSetCustomWallpaperLayoutFunction() {}

ExtensionFunction::ResponseAction
WallpaperPrivateSetCustomWallpaperLayoutFunction::Run() {
  std::unique_ptr<set_custom_wallpaper_layout::Params> params(
      set_custom_wallpaper_layout::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::WallpaperLayout new_layout = wallpaper_api_util::GetLayoutEnum(
      wallpaper_base::ToString(params->layout));
  wallpaper_api_util::RecordCustomWallpaperLayout(new_layout);
  WallpaperControllerClientImpl::Get()->UpdateCustomWallpaperLayout(
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId(),
      new_layout);
  return RespondNow(NoArguments());
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

WallpaperPrivateMinimizeInactiveWindowsFunction::
    ~WallpaperPrivateMinimizeInactiveWindowsFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateMinimizeInactiveWindowsFunction::Run() {
  WallpaperControllerClientImpl::Get()->MinimizeInactiveWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  return RespondNow(NoArguments());
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

WallpaperPrivateRestoreMinimizedWindowsFunction::
    ~WallpaperPrivateRestoreMinimizedWindowsFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateRestoreMinimizedWindowsFunction::Run() {
  WallpaperControllerClientImpl::Get()->RestoreMinimizedWindows(
      user_manager::UserManager::Get()->GetActiveUser()->username_hash());
  return RespondNow(NoArguments());
}

WallpaperPrivateGetThumbnailFunction::WallpaperPrivateGetThumbnailFunction() {
}

WallpaperPrivateGetThumbnailFunction::~WallpaperPrivateGetThumbnailFunction() {
}

ExtensionFunction::ResponseAction WallpaperPrivateGetThumbnailFunction::Run() {
  std::unique_ptr<get_thumbnail::Params> params(
      get_thumbnail::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath thumbnail_path;
  if (params->source == wallpaper_private::WALLPAPER_SOURCE_ONLINE) {
    std::string file_name = GURL(params->url_or_file).ExtractFileName();
    CHECK(base::PathService::Get(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS,
                                 &thumbnail_path));
    thumbnail_path = thumbnail_path.Append(file_name);
  } else {
    if (!IsOEMDefaultWallpaper())
      return RespondNow(Error("No OEM wallpaper."));

    // TODO(bshe): Small resolution wallpaper is used here as wallpaper
    // thumbnail. We should either resize it or include a wallpaper thumbnail in
    // addition to large and small wallpaper resolutions.
    thumbnail_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        chromeos::switches::kDefaultWallpaperSmall);
  }

  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&WallpaperPrivateGetThumbnailFunction::Get,
                                this, thumbnail_path));
  // WallpaperPrivateGetThumbnailFunction::Get will respond on UI thread
  // asynchronously.
  return RespondLater();
}

void WallpaperPrivateGetThumbnailFunction::Failure(
    const std::string& file_name) {
  Respond(Error(base::StringPrintf(
      "Failed to access wallpaper thumbnails for %s.", file_name.c_str())));
}

void WallpaperPrivateGetThumbnailFunction::FileNotLoaded() {
  // TODO(https://crbug.com/829657): This should fail instead of succeeding.
  Respond(NoArguments());
}

void WallpaperPrivateGetThumbnailFunction::FileLoaded(
    const std::string& data) {
  Respond(OneArgument(Value(base::as_bytes(base::make_span(data)))));
}

void WallpaperPrivateGetThumbnailFunction::Get(const base::FilePath& path) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  std::string data;
  if (GetData(path, &data)) {
    if (data.empty()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&WallpaperPrivateGetThumbnailFunction::FileNotLoaded,
                         this));
    } else {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&WallpaperPrivateGetThumbnailFunction::FileLoaded,
                         this, std::move(data)));
    }
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WallpaperPrivateGetThumbnailFunction::Failure, this,
                       path.BaseName().value()));
  }
}

WallpaperPrivateSaveThumbnailFunction::WallpaperPrivateSaveThumbnailFunction() {
}

WallpaperPrivateSaveThumbnailFunction::
    ~WallpaperPrivateSaveThumbnailFunction() {}

ExtensionFunction::ResponseAction WallpaperPrivateSaveThumbnailFunction::Run() {
  std::unique_ptr<save_thumbnail::Params> params(
      save_thumbnail::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  WallpaperFunctionBase::GetNonBlockingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Save, this,
                     params->data, GURL(params->url).ExtractFileName()));
  // WallpaperPrivateSaveThumbnailFunction::Save will repsond on UI thread
  // asynchronously.
  return RespondLater();
}

void WallpaperPrivateSaveThumbnailFunction::Failure(
    const std::string& file_name) {
  Respond(Error(base::StringPrintf("Failed to create/write thumbnail of %s.",
                                   file_name.c_str())));
}

void WallpaperPrivateSaveThumbnailFunction::Success() {
  Respond(NoArguments());
}

void WallpaperPrivateSaveThumbnailFunction::Save(
    const std::vector<uint8_t>& data,
    const std::string& file_name) {
  WallpaperFunctionBase::AssertCalledOnWallpaperSequence(
      WallpaperFunctionBase::GetNonBlockingTaskRunner());
  if (SaveData(chrome::DIR_CHROMEOS_WALLPAPER_THUMBNAILS, file_name, data)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Success, this));
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WallpaperPrivateSaveThumbnailFunction::Failure, this,
                       file_name));
  }
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    WallpaperPrivateGetOfflineWallpaperListFunction() {
}

WallpaperPrivateGetOfflineWallpaperListFunction::
    ~WallpaperPrivateGetOfflineWallpaperListFunction() {
}

ExtensionFunction::ResponseAction
WallpaperPrivateGetOfflineWallpaperListFunction::Run() {
  WallpaperControllerClientImpl::Get()->GetOfflineWallpaperList(
      base::BindOnce(&WallpaperPrivateGetOfflineWallpaperListFunction::
                         OnOfflineWallpaperListReturned,
                     this));
  return RespondLater();
}

void WallpaperPrivateGetOfflineWallpaperListFunction::
    OnOfflineWallpaperListReturned(const std::vector<std::string>& url_list) {
  base::Value results(base::Value::Type::LIST);
  for (const std::string& url : url_list) {
    results.Append(url);
  }
  Respond(OneArgument(std::move(results)));
}

ExtensionFunction::ResponseAction
WallpaperPrivateRecordWallpaperUMAFunction::Run() {
  // Removed UMA recording code for Ash.Wallpaper.Source metric
  // to not be triggered by the old Wallpaper Picker
  return RespondNow(NoArguments());
}

WallpaperPrivateGetCollectionsInfoFunction::
    WallpaperPrivateGetCollectionsInfoFunction() = default;

WallpaperPrivateGetCollectionsInfoFunction::
    ~WallpaperPrivateGetCollectionsInfoFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetCollectionsInfoFunction::Run() {
  collection_info_fetcher_ =
      std::make_unique<wallpaper_handlers::BackdropCollectionInfoFetcher>();
  collection_info_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetCollectionsInfoFunction::OnCollectionsInfoFetched,
      this));
  return RespondLater();
}

void WallpaperPrivateGetCollectionsInfoFunction::OnCollectionsInfoFetched(
    bool success,
    const std::vector<backdrop::Collection>& collections) {
  if (!success) {
    Respond(Error("Collection names are not available."));
    return;
  }

  std::vector<extensions::api::wallpaper_private::CollectionInfo>
      collections_info_list;
  for (const auto& collection : collections) {
    extensions::api::wallpaper_private::CollectionInfo collection_info;
    collection_info.collection_name = collection.collection_name();
    collection_info.collection_id = collection.collection_id();
    collections_info_list.push_back(std::move(collection_info));
  }
  Respond(ArgumentList(
      get_collections_info::Results::Create(collections_info_list)));
}

WallpaperPrivateGetImagesInfoFunction::WallpaperPrivateGetImagesInfoFunction() =
    default;

WallpaperPrivateGetImagesInfoFunction::
    ~WallpaperPrivateGetImagesInfoFunction() = default;

ExtensionFunction::ResponseAction WallpaperPrivateGetImagesInfoFunction::Run() {
  std::unique_ptr<get_images_info::Params> params(
      get_images_info::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  image_info_fetcher_ =
      std::make_unique<wallpaper_handlers::BackdropImageInfoFetcher>(
          params->collection_id);
  image_info_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetImagesInfoFunction::OnImagesInfoFetched, this));
  return RespondLater();
}

void WallpaperPrivateGetImagesInfoFunction::OnImagesInfoFetched(
    bool success,
    const std::string& collection_id,
    const std::vector<backdrop::Image>& images) {
  if (!success) {
    Respond(Error("Images info is not available."));
    return;
  }

  std::vector<extensions::api::wallpaper_private::ImageInfo> images_info_list;
  for (const auto& image : images) {
    extensions::api::wallpaper_private::ImageInfo image_info;
    ParseImageInfo(image, &image_info);
    images_info_list.push_back(std::move(image_info));
  }
  Respond(ArgumentList(get_images_info::Results::Create(images_info_list)));
}

WallpaperPrivateGetLocalImagePathsFunction::
    WallpaperPrivateGetLocalImagePathsFunction() = default;

WallpaperPrivateGetLocalImagePathsFunction::
    ~WallpaperPrivateGetLocalImagePathsFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetLocalImagePathsFunction::Run() {
  ash::EnumerateLocalWallpaperFiles(
      Profile::FromBrowserContext(browser_context()),
      base::BindOnce(
          &WallpaperPrivateGetLocalImagePathsFunction::OnGetImagePathsComplete,
          this));
  return RespondLater();
}

void WallpaperPrivateGetLocalImagePathsFunction::OnGetImagePathsComplete(
    const std::vector<base::FilePath>& image_paths) {
  std::vector<std::string> result;
  std::transform(image_paths.begin(), image_paths.end(),
                 std::back_inserter(result),
                 [](const base::FilePath& path) { return path.value(); });
  Respond(ArgumentList(get_local_image_paths::Results::Create(result)));
}

WallpaperPrivateGetLocalImageDataFunction::
    WallpaperPrivateGetLocalImageDataFunction() = default;

WallpaperPrivateGetLocalImageDataFunction::
    ~WallpaperPrivateGetLocalImageDataFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetLocalImageDataFunction::Run() {
  std::unique_ptr<get_local_image_data::Params> params(
      get_local_image_data::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(crbug.com/811564): Create file backed blob instead.
  auto image_data = std::make_unique<std::string>();
  std::string* image_data_ptr = image_data.get();
  base::PostTaskAndReplyWithResult(
      WallpaperFunctionBase::GetNonBlockingTaskRunner(), FROM_HERE,
      base::BindOnce(&base::ReadFileToString,
                     base::FilePath(params->image_path), image_data_ptr),
      base::BindOnce(
          &WallpaperPrivateGetLocalImageDataFunction::OnReadImageDataComplete,
          this, std::move(image_data)));

  return RespondLater();
}

void WallpaperPrivateGetLocalImageDataFunction::OnReadImageDataComplete(
    std::unique_ptr<std::string> image_data,
    bool success) {
  if (!success) {
    Respond(Error("Reading image data failed."));
    return;
  }

  Respond(ArgumentList(get_local_image_data::Results::Create(
      std::vector<uint8_t>(image_data->begin(), image_data->end()))));
}

WallpaperPrivateConfirmPreviewWallpaperFunction::
    WallpaperPrivateConfirmPreviewWallpaperFunction() = default;

WallpaperPrivateConfirmPreviewWallpaperFunction::
    ~WallpaperPrivateConfirmPreviewWallpaperFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateConfirmPreviewWallpaperFunction::Run() {
  WallpaperControllerClientImpl::Get()->ConfirmPreviewWallpaper();
  return RespondNow(NoArguments());
}

WallpaperPrivateCancelPreviewWallpaperFunction::
    WallpaperPrivateCancelPreviewWallpaperFunction() = default;

WallpaperPrivateCancelPreviewWallpaperFunction::
    ~WallpaperPrivateCancelPreviewWallpaperFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateCancelPreviewWallpaperFunction::Run() {
  WallpaperControllerClientImpl::Get()->CancelPreviewWallpaper();
  return RespondNow(NoArguments());
}

WallpaperPrivateGetCurrentWallpaperThumbnailFunction::
    WallpaperPrivateGetCurrentWallpaperThumbnailFunction() = default;

WallpaperPrivateGetCurrentWallpaperThumbnailFunction::
    ~WallpaperPrivateGetCurrentWallpaperThumbnailFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetCurrentWallpaperThumbnailFunction::Run() {
  std::unique_ptr<get_current_wallpaper_thumbnail::Params> params(
      get_current_wallpaper_thumbnail::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto image = WallpaperControllerClientImpl::Get()->GetWallpaperImage();
  gfx::Size thumbnail_size(params->thumbnail_width, params->thumbnail_height);
  image.EnsureRepsForSupportedScales();
  std::vector<uint8_t> thumbnail_data =
      GenerateThumbnail(image, thumbnail_size);
  return RespondNow(OneArgument(base::Value(std::move(thumbnail_data))));
}

void WallpaperPrivateGetCurrentWallpaperThumbnailFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& wallpaper) {}

WallpaperPrivateGetSurpriseMeImageFunction::
    WallpaperPrivateGetSurpriseMeImageFunction() = default;

WallpaperPrivateGetSurpriseMeImageFunction::
    ~WallpaperPrivateGetSurpriseMeImageFunction() = default;

ExtensionFunction::ResponseAction
WallpaperPrivateGetSurpriseMeImageFunction::Run() {
  std::unique_ptr<get_surprise_me_image::Params> params(
      get_surprise_me_image::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  surprise_me_image_fetcher_ =
      std::make_unique<wallpaper_handlers::BackdropSurpriseMeImageFetcher>(
          params->collection_id,
          params->resume_token ? *params->resume_token : std::string());
  surprise_me_image_fetcher_->Start(base::BindOnce(
      &WallpaperPrivateGetSurpriseMeImageFunction::OnSurpriseMeImageFetched,
      this));
  return RespondLater();
}

void WallpaperPrivateGetSurpriseMeImageFunction::OnSurpriseMeImageFetched(
    bool success,
    const backdrop::Image& image,
    const std::string& next_resume_token) {
  if (!success) {
    Respond(Error("Image not available."));
    return;
  }

  extensions::api::wallpaper_private::ImageInfo image_info;
  ParseImageInfo(image, &image_info);
  Respond(TwoArguments(Value::FromUniquePtrValue(image_info.ToValue()),
                       Value(next_resume_token)));
}

ExtensionFunction::ResponseAction WallpaperPrivateIsSwaEnabledFunction::Run() {
  return RespondNow(
      OneArgument(base::Value(ash::features::IsWallpaperWebUIEnabled())));
}
