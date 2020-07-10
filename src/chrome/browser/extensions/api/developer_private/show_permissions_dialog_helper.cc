// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/show_permissions_dialog_helper.h"

#include <utility>

#include "apps/saved_files_service.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

ShowPermissionsDialogHelper::ShowPermissionsDialogHelper(
    Profile* profile,
    const base::Closure& on_complete)
    : profile_(profile),
      on_complete_(on_complete) {
}

ShowPermissionsDialogHelper::~ShowPermissionsDialogHelper() {
}

// static
void ShowPermissionsDialogHelper::Show(content::BrowserContext* browser_context,
                                       content::WebContents* web_contents,
                                       const Extension* extension,
                                       bool from_webui,
                                       const base::Closure& on_complete) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Show the new-style extensions dialog when it is available. It is currently
  // unavailable by default on Mac.
  if (CanPlatformShowAppInfoDialog()) {
    if (from_webui) {
      UMA_HISTOGRAM_ENUMERATION("Apps.AppInfoDialog.Launches",
                                AppInfoLaunchSource::FROM_EXTENSIONS_PAGE,
                                AppInfoLaunchSource::NUM_LAUNCH_SOURCES);
    }

    ShowAppInfoInNativeDialog(web_contents, profile, extension, on_complete);

    return;  // All done.
  }

  // ShowPermissionsDialogHelper manages its own lifetime.
  ShowPermissionsDialogHelper* helper =
      new ShowPermissionsDialogHelper(profile, on_complete);
  helper->ShowPermissionsDialog(web_contents, extension);
}

void ShowPermissionsDialogHelper::ShowPermissionsDialog(
    content::WebContents* web_contents,
    const Extension* extension) {
  extension_id_ = extension->id();
  prompt_.reset(new ExtensionInstallPrompt(web_contents));
  std::vector<base::FilePath> retained_file_paths;
  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kFileSystem)) {
    std::vector<SavedFileEntry> retained_file_entries =
        apps::SavedFilesService::Get(profile_)->GetAllFileEntries(
            extension_id_);
    for (const SavedFileEntry& entry : retained_file_entries)
      retained_file_paths.push_back(entry.path);
  }
  std::vector<base::string16> retained_device_messages;
  if (extension->permissions_data()->HasAPIPermission(APIPermission::kUsb)) {
    retained_device_messages =
        DevicePermissionsManager::Get(profile_)
            ->GetPermissionMessageStrings(extension_id_);
  }
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::POST_INSTALL_PERMISSIONS_PROMPT));
  prompt->set_retained_files(retained_file_paths);
  prompt->set_retained_device_messages(retained_device_messages);
  // Unretained() is safe because this class manages its own lifetime and
  // deletes itself in OnInstallPromptDone().
  prompt_->ShowDialog(
      base::Bind(&ShowPermissionsDialogHelper::OnInstallPromptDone,
                 base::Unretained(this)),
      extension, nullptr, std::move(prompt),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void ShowPermissionsDialogHelper::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  if (result == ExtensionInstallPrompt::Result::ACCEPTED) {
    // This is true when the user clicks "Revoke File Access."
    const Extension* extension =
        ExtensionRegistry::Get(profile_)
            ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);

    if (extension)
      apps::SavedFilesService::Get(profile_)->ClearQueue(extension);
    apps::AppLoadService::Get(profile_)
        ->RestartApplicationIfRunning(extension_id_);
  }

  on_complete_.Run();
  delete this;
}

}  // namespace extensions
