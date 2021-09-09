// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ash {

namespace {

// "root" is appended to the user's Google Drive directory to form the
// complete path.
constexpr char kRoot[] = "root";

// The name of the sticky settings pref.
constexpr char kScanningStickySettingsPref[] =
    "scanning.scanning_sticky_settings";

}  // namespace

ChromeScanningAppDelegate::ChromeScanningAppDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {
  my_files_path_ = file_manager::util::GetMyFilesFolderForProfile(
      Profile::FromWebUI(web_ui));

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui_));
  const bool drive_available = integration_service &&
                               integration_service->is_enabled() &&
                               integration_service->IsMounted();
  google_drive_path_ = drive_available
                           ? integration_service->GetMountPointPath()
                           : base::FilePath();
}

ChromeScanningAppDelegate::~ChromeScanningAppDelegate() = default;

std::unique_ptr<ui::SelectFilePolicy>
ChromeScanningAppDelegate::CreateChromeSelectFilePolicy() {
  return std::make_unique<ChromeSelectFilePolicy>(web_ui_->GetWebContents());
}

std::string ChromeScanningAppDelegate::GetBaseNameFromPath(
    const base::FilePath& path) {
  // Returns string "Google Drive" from path "/media/fuse/drivefs-xxx/root".
  if (google_drive_path_.Append(kRoot) == path)
    return l10n_util::GetStringUTF8(IDS_SCANNING_APP_MY_DRIVE);

  // Returns string "My Files" from path "/home/chronos/u-xxx/MyFiles".
  if (my_files_path_ == path)
    return l10n_util::GetStringUTF8(IDS_SCANNING_APP_MY_FILES_SELECT_OPTION);

  // Returns base name as is from |path|.
  return path.BaseName().value();
}

base::FilePath ChromeScanningAppDelegate::GetMyFilesPath() {
  return my_files_path_;
}

std::string ChromeScanningAppDelegate::GetScanSettingsFromPrefs() {
  return GetPrefs()->GetString(kScanningStickySettingsPref);
}

// Determines if |path_to_file| is a supported file path for the Files app. Only
// files under the |drive_path_| and |my_files_path_| paths are allowed to be
// opened to from the Scan app. Paths with references (i.e. "../path") are not
// supported.
bool ChromeScanningAppDelegate::IsFilePathSupported(
    const base::FilePath& path_to_file) {
  return !path_to_file.ReferencesParent() &&
         (google_drive_path_.IsParent(path_to_file) ||
          my_files_path_.IsParent(path_to_file));
}

void ChromeScanningAppDelegate::OpenFilesInMediaApp(
    const std::vector<base::FilePath>& file_paths) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kScanAppMediaLink))
    return;

  DCHECK(!file_paths.empty());

  web_app::SystemAppLaunchParams params;
  params.launch_paths = file_paths;
  params.launch_source = apps::mojom::LaunchSource::kFromOtherApp;
  web_app::LaunchSystemWebAppAsync(Profile::FromWebUI(web_ui_),
                                   web_app::SystemAppType::MEDIA, params);
}

void ChromeScanningAppDelegate::SaveScanSettingsToPrefs(
    const std::string& scan_settings) {
  GetPrefs()->SetString(kScanningStickySettingsPref, scan_settings);
}

bool ChromeScanningAppDelegate::ShowFileInFilesApp(
    const base::FilePath& path_to_file) {
  const bool can_show_file_picker =
      IsFilePathSupported(path_to_file) && base::PathExists(path_to_file);
  if (!can_show_file_picker)
    return false;

  platform_util::ShowItemInFolder(Profile::FromWebUI(web_ui_), path_to_file);
  return true;
}

// static
void ChromeScanningAppDelegate::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kScanningStickySettingsPref, std::string());
}

void ChromeScanningAppDelegate::SetGoogleDrivePathForTesting(
    const base::FilePath& google_drive_path) {
  google_drive_path_ = google_drive_path;
}

void ChromeScanningAppDelegate::SetMyFilesPathForTesting(
    const base::FilePath& my_files_path) {
  my_files_path_ = my_files_path;
}

PrefService* ChromeScanningAppDelegate::GetPrefs() const {
  return Profile::FromWebUI(web_ui_)->GetPrefs();
}

}  // namespace ash
