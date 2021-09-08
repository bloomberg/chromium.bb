// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_CHROME_SCANNING_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SCANNING_CHROME_SCANNING_APP_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/content/scanning/scanning_app_delegate.h"
#include "base/files/file_path.h"

class PrefService;

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class SelectFilePolicy;
}  // namespace ui

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace ash {

class ChromeScanningAppDelegate : public ScanningAppDelegate {
 public:
  explicit ChromeScanningAppDelegate(content::WebUI* web_ui);
  ~ChromeScanningAppDelegate() override;

  ChromeScanningAppDelegate(const ChromeScanningAppDelegate&) = delete;
  ChromeScanningAppDelegate& operator=(const ChromeScanningAppDelegate&) =
      delete;

  // ScanningAppDelegate:
  std::unique_ptr<ui::SelectFilePolicy> CreateChromeSelectFilePolicy() override;
  std::string GetBaseNameFromPath(const base::FilePath& path) override;
  base::FilePath GetMyFilesPath() override;
  std::string GetScanSettingsFromPrefs() override;
  bool IsFilePathSupported(const base::FilePath& path_to_file) override;
  void OpenFilesInMediaApp(
      const std::vector<base::FilePath>& file_paths) override;
  void SaveScanSettingsToPrefs(const std::string& scan_settings) override;
  bool ShowFileInFilesApp(const base::FilePath& path_to_file) override;

  // Register scan settings prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Sets |google_drive_path_| for tests.
  void SetGoogleDrivePathForTesting(const base::FilePath& google_drive_path);

  // Sets |my_files_path_| for tests.
  void SetMyFilesPathForTesting(const base::FilePath& my_files_path);

 private:
  // Returns the PrefService for the active Profile.
  PrefService* GetPrefs() const;

  content::WebUI* web_ui_;  // Owns |this|.

  // The paths to the user's My files and Google Drive directories.
  base::FilePath google_drive_path_;
  base::FilePath my_files_path_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_CHROME_SCANNING_APP_DELEGATE_H_
