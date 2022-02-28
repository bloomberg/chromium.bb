// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_

#include "chrome/browser/android/chrome_backup_watcher.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile_manager_android.h"

class ChromeBrowserMainPartsAndroid : public ChromeBrowserMainParts {
 public:
  ChromeBrowserMainPartsAndroid(content::MainFunctionParams parameters,
                                StartupData* startup_data);

  ChromeBrowserMainPartsAndroid(const ChromeBrowserMainPartsAndroid&) = delete;
  ChromeBrowserMainPartsAndroid& operator=(
      const ChromeBrowserMainPartsAndroid&) = delete;

  ~ChromeBrowserMainPartsAndroid() override;

  // content::BrowserMainParts overrides.
  int PreCreateThreads() override;
  void PostProfileInit() override;
  int PreEarlyInitialization() override;
  void PostEarlyInitialization() override;

  // ChromeBrowserMainParts overrides.
  void PostBrowserStart() override;
  void ShowMissingLocaleMessageBox() override;

 private:
  std::unique_ptr<android::ChromeBackupWatcher> backup_watcher_;
  std::unique_ptr<ProfileManagerAndroid> profile_manager_android_;
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
