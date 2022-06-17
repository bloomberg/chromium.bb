// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_DEMO_MODE_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_DEMO_MODE_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"

struct WebAppInstallInfo;

#if defined(OFFICIAL_BUILD)
#error Demo Mode should only be included in unofficial builds.
#endif

class DemoModeSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit DemoModeSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
};

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForDemoModeApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_DEMO_MODE_WEB_APP_INFO_H_
