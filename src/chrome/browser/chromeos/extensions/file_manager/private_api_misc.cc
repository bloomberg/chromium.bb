// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_misc.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>
#include <vector>

#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/encoding_detection.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes_util.h"
#include "chrome/common/extensions/api/file_manager_private_internal.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "chromeos/settings/timezone_settings.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/event_logger.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/zoom/page_zoom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "google_apis/drive/auth_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace extensions {
namespace {

using api::file_manager_private::ProfileInfo;

const char kCWSScope[] = "https://www.googleapis.com/auth/chromewebstore";

// Thresholds for mountCrostini() API.
constexpr base::TimeDelta kMountCrostiniSlowOperationThreshold =
    base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kMountCrostiniVerySlowOperationThreshold =
    base::TimeDelta::FromSeconds(30);

// Obtains the current app window.
AppWindow* GetCurrentAppWindow(ExtensionFunction* function) {
  content::WebContents* const contents = function->GetSenderWebContents();
  return contents ? AppWindowRegistry::Get(function->browser_context())
                        ->GetAppWindowForWebContents(contents)
                  : nullptr;
}

std::vector<ProfileInfo> GetLoggedInProfileInfoList() {
  DCHECK(user_manager::UserManager::IsInitialized());
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::set<Profile*> original_profiles;
  std::vector<ProfileInfo> result_profiles;

  for (Profile* profile : profiles) {
    // Filter the profile.
    profile = profile->GetOriginalProfile();
    if (original_profiles.count(profile))
      continue;
    original_profiles.insert(profile);
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
    if (!user || !user->is_logged_in())
      continue;

    // Make a ProfileInfo.
    ProfileInfo profile_info;
    profile_info.profile_id =
        multi_user_util::GetAccountIdFromProfile(profile).GetUserEmail();
    profile_info.display_name = base::UTF16ToUTF8(user->GetDisplayName());
    // TODO(hirono): Remove the property from the profile_info.
    profile_info.is_current_profile = true;

    result_profiles.push_back(std::move(profile_info));
  }

  return result_profiles;
}

// Converts a list of file system urls (as strings) to a pair of a provided file
// system object and a list of unique paths on the file system. In case of an
// error, false is returned and the error message set.
bool ConvertURLsToProvidedInfo(
    const scoped_refptr<storage::FileSystemContext>& file_system_context,
    const std::vector<std::string>& urls,
    ash::file_system_provider::ProvidedFileSystemInterface** file_system,
    std::vector<base::FilePath>* paths,
    std::string* error) {
  DCHECK(file_system);
  DCHECK(error);

  if (urls.empty()) {
    *error = "At least one file must be specified.";
    return false;
  }

  *file_system = nullptr;
  for (const auto& url : urls) {
    const storage::FileSystemURL file_system_url(
        file_system_context->CrackURL(GURL(url)));

    ash::file_system_provider::util::FileSystemURLParser parser(
        file_system_url);
    if (!parser.Parse()) {
      *error = "Related provided file system not found.";
      return false;
    }

    if (*file_system != nullptr) {
      if (*file_system != parser.file_system()) {
        *error = "All entries must be on the same file system.";
        return false;
      }
    } else {
      *file_system = parser.file_system();
    }
    paths->push_back(parser.file_path());
  }

  // Erase duplicates.
  std::sort(paths->begin(), paths->end());
  paths->erase(std::unique(paths->begin(), paths->end()), paths->end());

  return true;
}

bool IsAllowedSource(storage::FileSystemType type,
                     api::file_manager_private::SourceRestriction restriction) {
  switch (restriction) {
    case api::file_manager_private::SOURCE_RESTRICTION_NONE:
      NOTREACHED();
      return false;

    case api::file_manager_private::SOURCE_RESTRICTION_ANY_SOURCE:
      return true;

    case api::file_manager_private::SOURCE_RESTRICTION_NATIVE_SOURCE:
      return type == storage::kFileSystemTypeLocal;
  }
}

std::string Redact(const std::string& s) {
  return LOG_IS_ON(INFO) ? base::StrCat({"'", s, "'"}) : "(redacted)";
}

std::string Redact(const base::FilePath& path) {
  return Redact(path.value());
}

}  // namespace

ExtensionFunction::ResponseAction
FileManagerPrivateLogoutUserForReauthenticationFunction::Run() {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(browser_context()));
  if (user) {
    user_manager::UserManager::Get()->SaveUserOAuthStatus(
        user->GetAccountId(), user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);
  }

  chrome::AttemptUserExit();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateGetPreferencesFunction::Run() {
  api::file_manager_private::Preferences result;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  const PrefService* const service = profile->GetPrefs();
  auto* drive_integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);

  result.drive_enabled = drive::util::IsDriveEnabledForProfile(profile) &&
                         drive_integration_service &&
                         !drive_integration_service->mount_failed();
  result.cellular_disabled =
      service->GetBoolean(drive::prefs::kDisableDriveOverCellular);
  result.search_suggest_enabled =
      service->GetBoolean(prefs::kSearchSuggestEnabled);
  result.use24hour_clock = service->GetBoolean(prefs::kUse24HourClock);
  result.timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());
  result.arc_enabled = service->GetBoolean(arc::prefs::kArcEnabled);
  result.arc_removable_media_access_enabled =
      service->GetBoolean(arc::prefs::kArcHasAccessToRemovableMedia);

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(result.ToValue())));
}

ExtensionFunction::ResponseAction
FileManagerPrivateSetPreferencesFunction::Run() {
  using extensions::api::file_manager_private::SetPreferences::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* const service = profile->GetPrefs();

  if (params->change_info.cellular_disabled) {
    service->SetBoolean(drive::prefs::kDisableDriveOverCellular,
                        *params->change_info.cellular_disabled);
  }
  if (params->change_info.arc_enabled) {
    service->SetBoolean(arc::prefs::kArcEnabled,
                        *params->change_info.arc_enabled);
  }
  if (params->change_info.arc_removable_media_access_enabled) {
    service->SetBoolean(
        arc::prefs::kArcHasAccessToRemovableMedia,
        *params->change_info.arc_removable_media_access_enabled);
  }

  return RespondNow(NoArguments());
}

// Collection of active ZipFileCreator objects, indexed by ZIP file path.
using ZipCreators =
    std::unordered_map<base::FilePath, scoped_refptr<ZipFileCreator>>;
static base::NoDestructor<ZipCreators> zip_creators;

FileManagerPrivateInternalZipSelectionFunction::
    FileManagerPrivateInternalZipSelectionFunction() = default;

FileManagerPrivateInternalZipSelectionFunction::
    ~FileManagerPrivateInternalZipSelectionFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalZipSelectionFunction::Run() {
  using extensions::api::file_manager_private_internal::ZipSelection::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());

  // Convert parent directory URL to absolute path.
  if (params->parent_url.empty())
    return RespondNow(Error("Empty parent URL"));

  const base::FilePath parent_dir = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), profile, GURL(params->parent_url));
  if (parent_dir.empty())
    return RespondNow(
        Error(base::StrCat({"Cannot convert parent URL ",
                            Redact(params->parent_url), " to absolute path"})));

  // Convert source file URLs to relative paths.
  if (params->urls.empty())
    return RespondNow(Error("No input files"));

  std::vector<base::FilePath> src_files;
  src_files.reserve(params->urls.size());

  for (const std::string& url : params->urls) {
    // Convert input URL to absolute path.
    const base::FilePath absolute_path =
        file_manager::util::GetLocalPathFromURL(render_frame_host(), profile,
                                                GURL(url));
    if (absolute_path.empty())
      return RespondNow(Error(base::StrCat(
          {"Cannot convert URL ", Redact(url), " to absolute file path"})));

    // Convert absolute path to relative path under |parent_dir|.
    base::FilePath relative_path;
    if (!parent_dir.AppendRelativePath(absolute_path, &relative_path))
      return RespondNow(
          Error(base::StrCat({"Input file ", Redact(absolute_path),
                              " is not in directory ", Redact(parent_dir)})));

    src_files.push_back(std::move(relative_path));
  }

  // Convert destination filename to absolute path.
  if (params->dest_name.empty())
    return RespondNow(Error("Empty destination file name"));

  const base::FilePath dest_file = parent_dir.Append(params->dest_name);

  VLOG(0) << "Creating ZIP archive " << Redact(dest_file) << " with "
          << src_files.size() << " items...";

  // Create a ZipFileCreator.
  scoped_refptr<ZipFileCreator>& creator = (*zip_creators)[dest_file];
  DCHECK(!creator);
  creator = base::MakeRefCounted<ZipFileCreator>(
      base::BindOnce(&FileManagerPrivateInternalZipSelectionFunction::OnZipDone,
                     this, dest_file),
      parent_dir, src_files, dest_file);

  // Start the ZipFileCreator.
  creator->Start(LaunchFileUtilService());

  return RespondLater();
}

void FileManagerPrivateInternalZipSelectionFunction::OnZipDone(
    const base::FilePath& dest_file,
    bool success) {
  if (success) {
    VLOG(0) << "Created ZIP archive " << Redact(dest_file);
  } else {
    LOG(ERROR) << "Cannot create ZIP archive " << Redact(dest_file);
  }

  Respond(OneArgument(base::Value(success)));

  // Remove the matching ZipFileCreator from the list of active ones.
  const size_t n = zip_creators->erase(dest_file);
  DCHECK_GT(n, 0);
}

FileManagerPrivateInternalCancelZipFunction::
    FileManagerPrivateInternalCancelZipFunction() = default;

FileManagerPrivateInternalCancelZipFunction::
    ~FileManagerPrivateInternalCancelZipFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalCancelZipFunction::Run() {
  using extensions::api::file_manager_private_internal::CancelZip::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());

  // Convert parent directory URL to absolute path.
  if (params->parent_url.empty())
    return RespondNow(Error("Empty parent URL"));

  const base::FilePath parent_dir = file_manager::util::GetLocalPathFromURL(
      render_frame_host(), profile, GURL(params->parent_url));
  if (parent_dir.empty())
    return RespondNow(
        Error(base::StrCat({"Cannot convert parent URL ",
                            Redact(params->parent_url), " to absolute path"})));

  // Convert destination filename to absolute path.
  if (params->dest_name.empty())
    return RespondNow(Error("Empty destination file name"));

  const base::FilePath dest_file = parent_dir.Append(params->dest_name);

  // Retrieve matching ZipFileCreator from the collection of active ones.
  const auto it = zip_creators->find(dest_file);
  if (it == zip_creators->end())
    return RespondNow(Error(base::StrCat(
        {"No ZIP operation currently running for ", Redact(dest_file)})));

  ZipFileCreator* const creator = it->second.get();
  DCHECK(creator);

  // Tell the ZipFileCreator to stop.
  creator->Stop();

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction FileManagerPrivateZoomFunction::Run() {
  using extensions::api::file_manager_private::Zoom::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  content::PageZoom zoom_type;
  switch (params->operation) {
    case api::file_manager_private::ZOOM_OPERATION_TYPE_IN:
      zoom_type = content::PAGE_ZOOM_IN;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_OUT:
      zoom_type = content::PAGE_ZOOM_OUT;
      break;
    case api::file_manager_private::ZOOM_OPERATION_TYPE_RESET:
      zoom_type = content::PAGE_ZOOM_RESET;
      break;
    default:
      NOTREACHED();
      return RespondNow(Error(kUnknownErrorDoNotUse));
  }
  zoom::PageZoom::Zoom(GetSenderWebContents(), zoom_type);
  return RespondNow(NoArguments());
}

FileManagerPrivateRequestWebStoreAccessTokenFunction::
    FileManagerPrivateRequestWebStoreAccessTokenFunction() = default;

FileManagerPrivateRequestWebStoreAccessTokenFunction::
    ~FileManagerPrivateRequestWebStoreAccessTokenFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateRequestWebStoreAccessTokenFunction::Run() {
  std::vector<std::string> scopes;
  scopes.emplace_back(kCWSScope);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager) {
    drive::EventLogger* logger = file_manager::util::GetLogger(profile);
    if (logger) {
      logger->Log(logging::LOG_ERROR,
                  "CWS Access token fetch failed. IdentityManager can't "
                  "be retrieved.");
    }
    return RespondNow(Error("Unable to fetch token."));
  }

  // "Unconsented" because this class doesn't care about browser sync consent.
  auth_service_ = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      scopes);
  auth_service_->StartAuthentication(
      base::BindOnce(&FileManagerPrivateRequestWebStoreAccessTokenFunction::
                         OnAccessTokenFetched,
                     this));

  return RespondLater();
}

void FileManagerPrivateRequestWebStoreAccessTokenFunction::OnAccessTokenFetched(
    google_apis::ApiErrorCode code,
    const std::string& access_token) {
  drive::EventLogger* logger = file_manager::util::GetLogger(
      Profile::FromBrowserContext(browser_context()));

  if (code == google_apis::HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    DCHECK(access_token == auth_service_->access_token());
    if (logger)
      logger->Log(logging::LOG_INFO, "CWS OAuth token fetch succeeded.");
    Respond(OneArgument(base::Value(access_token)));
  } else {
    if (logger) {
      logger->Log(logging::LOG_ERROR,
                  "CWS OAuth token fetch failed. (ApiErrorCode: %s)",
                  google_apis::ApiErrorCodeToString(code).c_str());
    }
    Respond(Error("Token fetch failed."));
  }
}

ExtensionFunction::ResponseAction FileManagerPrivateGetProfilesFunction::Run() {
  const std::vector<ProfileInfo>& profiles = GetLoggedInProfileInfoList();

  // Obtains the display profile ID.
  AppWindow* const app_window = GetCurrentAppWindow(this);
  ash::MultiUserWindowManager* const window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();
  const AccountId current_profile_id = multi_user_util::GetAccountIdFromProfile(
      Profile::FromBrowserContext(browser_context()));
  const AccountId display_profile_id =
      window_manager && app_window ? window_manager->GetUserPresentingWindow(
                                         app_window->GetNativeWindow())
                                   : EmptyAccountId();

  return RespondNow(
      ArgumentList(api::file_manager_private::GetProfiles::Results::Create(
          profiles, current_profile_id.GetUserEmail(),
          display_profile_id.is_valid() ? display_profile_id.GetUserEmail()
                                        : current_profile_id.GetUserEmail())));
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenInspectorFunction::Run() {
  using extensions::api::file_manager_private::OpenInspector::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  switch (params->type) {
    case extensions::api::file_manager_private::INSPECTION_TYPE_NORMAL:
      // Open inspector for foreground page.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_CONSOLE:
      // Open inspector for foreground page and bring focus to the console.
      DevToolsWindow::OpenDevToolsWindow(
          GetSenderWebContents(), DevToolsToggleAction::ShowConsolePanel());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_ELEMENT:
      // Open inspector for foreground page in inspect element mode.
      DevToolsWindow::OpenDevToolsWindow(GetSenderWebContents(),
                                         DevToolsToggleAction::Inspect());
      break;
    case extensions::api::file_manager_private::INSPECTION_TYPE_BACKGROUND:
      // Open inspector for background page.
      extensions::devtools_util::InspectBackgroundPage(
          extension(), Profile::FromBrowserContext(browser_context()));
      break;
    default:
      NOTREACHED();
      return RespondNow(Error(
          base::StringPrintf("Unexpected inspection type(%d) is specified.",
                             static_cast<int>(params->type))));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateOpenSettingsSubpageFunction::Run() {
  using extensions::api::file_manager_private::OpenSettingsSubpage::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (chromeos::settings::IsOSSettingsSubPage(params->sub_page)) {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, params->sub_page);
  } else {
    chrome::ShowSettingsSubPageForProfile(profile, params->sub_page);
  }
  return RespondNow(NoArguments());
}

FileManagerPrivateInternalGetMimeTypeFunction::
    FileManagerPrivateInternalGetMimeTypeFunction() = default;

FileManagerPrivateInternalGetMimeTypeFunction::
    ~FileManagerPrivateInternalGetMimeTypeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetMimeTypeFunction::Run() {
  using extensions::api::file_manager_private_internal::GetMimeType::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Convert file url to local path.
  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  storage::FileSystemURL file_system_url(
      file_system_context->CrackURL(GURL(params->url)));

  app_file_handler_util::GetMimeTypeForLocalPath(
      profile, file_system_url.path(),
      base::BindOnce(
          &FileManagerPrivateInternalGetMimeTypeFunction::OnGetMimeType, this));

  return RespondLater();
}

void FileManagerPrivateInternalGetMimeTypeFunction::OnGetMimeType(
    const std::string& mimeType) {
  Respond(OneArgument(base::Value(mimeType)));
}

FileManagerPrivateGetProvidersFunction::
    FileManagerPrivateGetProvidersFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateGetProvidersFunction::Run() {
  using ash::file_system_provider::Capabilities;
  using ash::file_system_provider::IconSet;
  using ash::file_system_provider::ProviderId;
  using ash::file_system_provider::ProviderInterface;
  using ash::file_system_provider::Service;
  const Service* const service = Service::Get(browser_context());

  using api::file_manager_private::Provider;
  std::vector<Provider> result;
  for (const auto& pair : service->GetProviders()) {
    const ProviderInterface* const provider = pair.second.get();
    const ProviderId provider_id = provider->GetId();

    Provider result_item;
    result_item.provider_id = provider->GetId().ToString();
    const IconSet& icon_set = provider->GetIconSet();
    file_manager::util::FillIconSet(&result_item.icon_set, icon_set);
    result_item.name = provider->GetName();

    const Capabilities capabilities = provider->GetCapabilities();
    result_item.configurable = capabilities.configurable;
    result_item.watchable = capabilities.watchable;
    result_item.multiple_mounts = capabilities.multiple_mounts;
    switch (capabilities.source) {
      case SOURCE_FILE:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_FILE;
        break;
      case SOURCE_DEVICE:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_DEVICE;
        break;
      case SOURCE_NETWORK:
        result_item.source = api::file_manager_private::PROVIDER_SOURCE_NETWORK;
        break;
    }
    result.push_back(std::move(result_item));
  }

  return RespondNow(ArgumentList(
      api::file_manager_private::GetProviders::Results::Create(result)));
}

FileManagerPrivateAddProvidedFileSystemFunction::
    FileManagerPrivateAddProvidedFileSystemFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateAddProvidedFileSystemFunction::Run() {
  using extensions::api::file_manager_private::AddProvidedFileSystem::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using ash::file_system_provider::ProviderId;
  using ash::file_system_provider::ProvidingExtensionInfo;
  using ash::file_system_provider::Service;
  Service* const service = Service::Get(browser_context());

  if (!service->RequestMount(ProviderId::FromString(params->provider_id)))
    return RespondNow(Error("Failed to request a new mount."));

  return RespondNow(NoArguments());
}

FileManagerPrivateConfigureVolumeFunction::
    FileManagerPrivateConfigureVolumeFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateConfigureVolumeFunction::Run() {
  using extensions::api::file_manager_private::ConfigureVolume::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::Volume;
  using file_manager::VolumeManager;
  VolumeManager* const volume_manager =
      VolumeManager::Get(Profile::FromBrowserContext(browser_context()));
  base::WeakPtr<Volume> volume =
      volume_manager->FindVolumeById(params->volume_id);
  if (!volume.get())
    return RespondNow(Error("Volume not found."));
  if (!volume->configurable())
    return RespondNow(Error("Volume not configurable."));

  switch (volume->type()) {
    case file_manager::VOLUME_TYPE_PROVIDED: {
      using ash::file_system_provider::Service;
      Service* const service = Service::Get(browser_context());
      DCHECK(service);

      using ash::file_system_provider::ProvidedFileSystemInterface;
      ProvidedFileSystemInterface* const file_system =
          service->GetProvidedFileSystem(volume->provider_id(),
                                         volume->file_system_id());
      if (file_system)
        file_system->Configure(base::BindOnce(
            &FileManagerPrivateConfigureVolumeFunction::OnCompleted, this));
      break;
    }
    default:
      NOTIMPLEMENTED();
  }

  return RespondLater();
}

void FileManagerPrivateConfigureVolumeFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to complete configuration."));
    return;
  }

  Respond(NoArguments());
}

FileManagerPrivateMountCrostiniFunction::
    FileManagerPrivateMountCrostiniFunction() {
  // Mounting crostini shares may require the crostini VM to be started.
  SetWarningThresholds(kMountCrostiniSlowOperationThreshold,
                       kMountCrostiniVerySlowOperationThreshold);
}

FileManagerPrivateMountCrostiniFunction::
    ~FileManagerPrivateMountCrostiniFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateMountCrostiniFunction::Run() {
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  DCHECK(crostini::CrostiniFeatures::Get()->IsEnabled(profile));
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      crostini::ContainerId::GetDefault(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::RestartCallback,
                     this));
  return RespondLater();
}

void FileManagerPrivateMountCrostiniFunction::RestartCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(
        base::StringPrintf("Error mounting crostini container: %d", result)));
    return;
  }
  // Use OriginalProfile since using crostini in incognito such as saving
  // files into Linux files should still work.
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  DCHECK(crostini::CrostiniFeatures::Get()->IsEnabled(profile));
  crostini::CrostiniManager::GetForProfile(profile)->MountCrostiniFiles(
      crostini::ContainerId::GetDefault(),
      base::BindOnce(&FileManagerPrivateMountCrostiniFunction::MountCallback,
                     this),
      false);
}

void FileManagerPrivateMountCrostiniFunction::MountCallback(
    crostini::CrostiniResult result) {
  if (result != crostini::CrostiniResult::SUCCESS) {
    Respond(Error(
        base::StringPrintf("Error mounting crostini container: %d", result)));
    return;
  }
  Respond(NoArguments());
}

FileManagerPrivateInternalImportCrostiniImageFunction::
    FileManagerPrivateInternalImportCrostiniImageFunction() = default;

FileManagerPrivateInternalImportCrostiniImageFunction::
    ~FileManagerPrivateInternalImportCrostiniImageFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalImportCrostiniImageFunction::Run() {
  using extensions::api::file_manager_private_internal::ImportCrostiniImage::
      Params;

  const auto params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  base::FilePath path = file_system_context->CrackURL(GURL(params->url)).path();

  crostini::CrostiniExportImport::GetForProfile(profile)->ImportContainer(
      crostini::ContainerId::GetDefault(), path,
      base::BindOnce(
          [](base::FilePath path, crostini::CrostiniResult result) {
            if (result != crostini::CrostiniResult::SUCCESS) {
              LOG(ERROR) << "Error importing crostini image " << Redact(path)
                         << ": " << (int)result;
            }
          },
          path));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalSharePathsWithCrostiniFunction::Run() {
  using extensions::api::file_manager_private_internal::SharePathsWithCrostini::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  std::vector<base::FilePath> paths;
  for (size_t i = 0; i < params->urls.size(); ++i) {
    storage::FileSystemURL cracked =
        file_system_context->CrackURL(GURL(params->urls[i]));
    paths.emplace_back(cracked.path());
  }

  guest_os::GuestOsSharePath::GetForProfile(profile)->SharePaths(
      params->vm_name, std::move(paths), params->persist,
      base::BindOnce(&FileManagerPrivateInternalSharePathsWithCrostiniFunction::
                         SharePathsCallback,
                     this));

  return RespondLater();
}

void FileManagerPrivateInternalSharePathsWithCrostiniFunction::
    SharePathsCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalUnsharePathWithCrostiniFunction::Run() {
  using extensions::api::file_manager_private_internal::
      UnsharePathWithCrostini::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());
  storage::FileSystemURL cracked =
      file_system_context->CrackURL(GURL(params->url));
  guest_os::GuestOsSharePath::GetForProfile(profile)->UnsharePath(
      params->vm_name, cracked.path(), /*unpersist=*/true,
      base::BindOnce(
          &FileManagerPrivateInternalUnsharePathWithCrostiniFunction::
              UnsharePathCallback,
          this));

  return RespondLater();
}

void FileManagerPrivateInternalUnsharePathWithCrostiniFunction::
    UnsharePathCallback(bool success, const std::string& failure_reason) {
  Respond(success ? NoArguments() : Error(failure_reason));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCrostiniSharedPathsFunction::Run() {
  using extensions::api::file_manager_private_internal::GetCrostiniSharedPaths::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  // TODO(crbug.com/1057591): Unexpected crashes in
  // GuestOsSharePath::GetPersistedSharedPaths with null profile_.
  CHECK(browser_context());
  Profile* profile = Profile::FromBrowserContext(browser_context());
  CHECK(profile);

  auto* guest_os_share_path =
      guest_os::GuestOsSharePath::GetForProfile(profile);
  CHECK(guest_os_share_path);
  bool first_for_session = params->observe_first_for_session &&
                           guest_os_share_path->GetAndSetFirstForSession();
  auto shared_paths =
      guest_os_share_path->GetPersistedSharedPaths(params->vm_name);
  auto entries = std::make_unique<base::ListValue>();
  for (const base::FilePath& path : shared_paths) {
    std::string mount_name;
    std::string file_system_name;
    std::string full_path;
    if (!file_manager::util::ExtractMountNameFileSystemNameFullPath(
            path, &mount_name, &file_system_name, &full_path)) {
      LOG(ERROR) << "Error extracting mount name and path from "
                 << Redact(path);
      continue;
    }
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString(
        "fileSystemRoot",
        storage::GetExternalFileSystemRootURIString(source_url(), mount_name));
    entry->SetString("fileSystemName", file_system_name);
    entry->SetString("fileFullPath", full_path);
    // All shared paths should be directories.  Even if this is not true,
    // it is fine for foreground/js/crostini.js class to think so. We
    // verify that the paths are in fact valid directories before calling
    // seneschal/9p in GuestOsSharePath::CallSeneschalSharePath().
    entry->SetBoolean("fileIsDirectory", true);
    entries->Append(std::move(entry));
  }
  return RespondNow(
      TwoArguments(base::Value::FromUniquePtrValue(std::move(entries)),
                   base::Value(first_for_session)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetLinuxPackageInfoFunction::Run() {
  using api::file_manager_private_internal::GetLinuxPackageInfo::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageService::GetForProfile(profile)->GetLinuxPackageInfo(
      crostini::ContainerId::GetDefault(),
      file_system_context->CrackURL(GURL(params->url)),
      base::BindOnce(&FileManagerPrivateInternalGetLinuxPackageInfoFunction::
                         OnGetLinuxPackageInfo,
                     this));
  return RespondLater();
}

void FileManagerPrivateInternalGetLinuxPackageInfoFunction::
    OnGetLinuxPackageInfo(
        const crostini::LinuxPackageInfo& linux_package_info) {
  api::file_manager_private::LinuxPackageInfo result;
  if (!linux_package_info.success) {
    Respond(Error(linux_package_info.failure_reason));
    return;
  }

  result.name = linux_package_info.name;
  result.version = linux_package_info.version;
  result.summary = std::make_unique<std::string>(linux_package_info.summary);
  result.description =
      std::make_unique<std::string>(linux_package_info.description);

  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           GetLinuxPackageInfo::Results::Create(result)));
}

ExtensionFunction::ResponseAction
FileManagerPrivateInternalInstallLinuxPackageFunction::Run() {
  using extensions::api::file_manager_private_internal::InstallLinuxPackage::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  crostini::CrostiniPackageService::GetForProfile(profile)
      ->QueueInstallLinuxPackage(
          crostini::ContainerId::GetDefault(),
          file_system_context->CrackURL(GURL(params->url)),
          base::BindOnce(
              &FileManagerPrivateInternalInstallLinuxPackageFunction::
                  OnInstallLinuxPackage,
              this));
  return RespondLater();
}

void FileManagerPrivateInternalInstallLinuxPackageFunction::
    OnInstallLinuxPackage(crostini::CrostiniResult result) {
  extensions::api::file_manager_private::InstallLinuxPackageResponse response;
  switch (result) {
    case crostini::CrostiniResult::SUCCESS:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_STARTED;
      break;
    case crostini::CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_FAILED;
      break;
    case crostini::CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE:
      response = extensions::api::file_manager_private::
          INSTALL_LINUX_PACKAGE_RESPONSE_INSTALL_ALREADY_ACTIVE;
      break;
    default:
      NOTREACHED();
  }
  Respond(ArgumentList(extensions::api::file_manager_private_internal::
                           InstallLinuxPackage::Results::Create(response)));
}

FileManagerPrivateInternalGetCustomActionsFunction::
    FileManagerPrivateInternalGetCustomActionsFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetCustomActionsFunction::Run() {
  using extensions::api::file_manager_private_internal::GetCustomActions::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<base::FilePath> paths;
  ash::file_system_provider::ProvidedFileSystemInterface* file_system = nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->GetActions(
      paths,
      base::BindOnce(
          &FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalGetCustomActionsFunction::OnCompleted(
    const ash::file_system_provider::Actions& actions,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to fetch actions."));
    return;
  }

  using api::file_system_provider::Action;
  std::vector<Action> items;
  for (const auto& action : actions) {
    Action item;
    item.id = action.id;
    item.title = std::make_unique<std::string>(action.title);
    items.push_back(std::move(item));
  }

  Respond(ArgumentList(
      api::file_manager_private_internal::GetCustomActions::Results::Create(
          items)));
}

FileManagerPrivateInternalExecuteCustomActionFunction::
    FileManagerPrivateInternalExecuteCustomActionFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalExecuteCustomActionFunction::Run() {
  using extensions::api::file_manager_private_internal::ExecuteCustomAction::
      Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          Profile::FromBrowserContext(browser_context()), render_frame_host());

  std::vector<base::FilePath> paths;
  ash::file_system_provider::ProvidedFileSystemInterface* file_system = nullptr;
  std::string error;

  if (!ConvertURLsToProvidedInfo(file_system_context, params->urls,
                                 &file_system, &paths, &error)) {
    return RespondNow(Error(error));
  }

  DCHECK(file_system);
  file_system->ExecuteAction(
      paths, params->action_id,
      base::BindOnce(
          &FileManagerPrivateInternalExecuteCustomActionFunction::OnCompleted,
          this));
  return RespondLater();
}

void FileManagerPrivateInternalExecuteCustomActionFunction::OnCompleted(
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    Respond(Error("Failed to execute the action."));
    return;
  }

  Respond(NoArguments());
}

FileManagerPrivateInternalGetRecentFilesFunction::
    FileManagerPrivateInternalGetRecentFilesFunction() = default;

ExtensionFunction::ResponseAction
FileManagerPrivateInternalGetRecentFilesFunction::Run() {
  using extensions::api::file_manager_private_internal::GetRecentFiles::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  const scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host());

  chromeos::RecentModel* model = chromeos::RecentModel::GetForProfile(profile);

  chromeos::RecentModel::FileType file_type;
  switch (params->file_type) {
    case api::file_manager_private::RECENT_FILE_TYPE_ALL:
      file_type = chromeos::RecentModel::FileType::kAll;
      break;
    case api::file_manager_private::RECENT_FILE_TYPE_AUDIO:
      file_type = chromeos::RecentModel::FileType::kAudio;
      break;
    case api::file_manager_private::RECENT_FILE_TYPE_IMAGE:
      file_type = chromeos::RecentModel::FileType::kImage;
      break;
    case api::file_manager_private::RECENT_FILE_TYPE_VIDEO:
      file_type = chromeos::RecentModel::FileType::kVideo;
      break;
    default:
      NOTREACHED();
      return RespondNow(Error("Unknown recent file type is specified."));
  }

  model->GetRecentFiles(
      file_system_context.get(), source_url(), file_type,
      base::BindOnce(
          &FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles,
          this, params->restriction));
  return RespondLater();
}

void FileManagerPrivateInternalGetRecentFilesFunction::OnGetRecentFiles(
    api::file_manager_private::SourceRestriction restriction,
    const std::vector<chromeos::RecentFile>& files) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  file_manager::util::FileDefinitionList file_definition_list;
  for (const auto& file : files) {
    // Filter out files from non-allowed sources.
    // We do this filtering here rather than in RecentModel so that the set of
    // files returned with some restriction is a subset of what would be
    // returned without restriction. Anyway, the maximum number of files
    // returned from RecentModel is large enough.
    if (!IsAllowedSource(file.url().type(), restriction))
      continue;

    file_manager::util::FileDefinition file_definition;
    // Recent file system only lists regular files, not directories.
    file_definition.is_directory = false;
    if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, source_url(), file.url().path(),
            &file_definition.virtual_path)) {
      file_definition_list.emplace_back(std::move(file_definition));
    }
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                           source_url()),
      url::Origin::Create(source_url().GetOrigin()),
      file_definition_list,  // Safe, since copied internally.
      base::BindOnce(&FileManagerPrivateInternalGetRecentFilesFunction::
                         OnConvertFileDefinitionListToEntryDefinitionList,
                     this));
}

void FileManagerPrivateInternalGetRecentFilesFunction::
    OnConvertFileDefinitionListToEntryDefinitionList(
        std::unique_ptr<file_manager::util::EntryDefinitionList>
            entry_definition_list) {
  DCHECK(entry_definition_list);

  Respond(OneArgument(base::Value::FromUniquePtrValue(
      file_manager::util::ConvertEntryDefinitionListToListValue(
          *entry_definition_list))));
}

ExtensionFunction::ResponseAction
FileManagerPrivateDetectCharacterEncodingFunction::Run() {
  using extensions::api::file_manager_private::DetectCharacterEncoding::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string input;
  if (!base::HexStringToString(params->bytes, &input))
    input.clear();

  std::string encoding;
  bool success = base::DetectEncoding(input, &encoding);
  return RespondNow(
      OneArgument(base::Value(success ? std::move(encoding) : std::string())));
}

ExtensionFunction::ResponseAction
FileManagerPrivateIsTabletModeEnabledFunction::Run() {
  ash::TabletMode* tablet_mode = ash::TabletMode::Get();
  return RespondNow(OneArgument(
      base::Value(tablet_mode ? tablet_mode->InTabletMode() : false)));
}

}  // namespace extensions
