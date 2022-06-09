// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PRINT_MANAGEMENT_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PRINT_MANAGEMENT_WEB_APP_INFO_H_

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "ui/gfx/geometry/size.h"

struct WebApplicationInfo;

class PrintManagementSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit PrintManagementSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  gfx::Size GetMinimumWindowSize() const override;
};

// Returns a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForPrintManagementApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PRINT_MANAGEMENT_WEB_APP_INFO_H_
