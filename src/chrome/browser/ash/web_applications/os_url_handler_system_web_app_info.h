// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_URL_HANDLER_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_URL_HANDLER_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"

class Profile;

// This is the web app handler which is called from Lacros and serves Ash's
// chrome:// URLs as web applications.
// To allow users to call Ash's pages directly, they can use os://<url> which
// will then be handled by this app.
class OsUrlHandlerSystemWebAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit OsUrlHandlerSystemWebAppDelegate(Profile* profile);
  OsUrlHandlerSystemWebAppDelegate(const OsUrlHandlerSystemWebAppDelegate&) =
      delete;
  OsUrlHandlerSystemWebAppDelegate operator=(
      const OsUrlHandlerSystemWebAppDelegate&) = delete;
  ~OsUrlHandlerSystemWebAppDelegate() override;

  // web_app::SystemWebAppDelegate:
  std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const override;

  // TODO(crbug/1260386) - Add override for GetAdditionalSearchTerms() to allow
  // capturing the os:// search tearms to be used.
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearch() const override;
  bool ShouldReuseExistingWindow() const override;
  bool IsUrlInSystemAppScope(const GURL& url) const override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_URL_HANDLER_SYSTEM_WEB_APP_INFO_H_
