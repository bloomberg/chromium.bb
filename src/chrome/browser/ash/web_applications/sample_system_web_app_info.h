// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SAMPLE_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SAMPLE_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

struct WebApplicationInfo;

#if defined(OFFICIAL_BUILD)
#error Demo Mode should only be included in unofficial builds.
#endif

class SampleSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit SampleSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldReuseExistingWindow() const override;
  bool ShouldShowNewWindowMenuOption() const override;
  absl::optional<web_app::SystemAppBackgroundTaskInfo> GetTimerInfo()
      const override;
};

// Return a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForSampleSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SAMPLE_SYSTEM_WEB_APP_INFO_H_
