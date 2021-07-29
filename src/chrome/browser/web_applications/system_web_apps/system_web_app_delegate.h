// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_DELEGATE_H_

#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

class Browser;

namespace web_app {
class SystemWebAppDelegate;

using OriginTrialsMap = std::map<url::Origin, std::vector<std::string>>;

// Use #if defined to avoid compiler error on unused function.
#if BUILDFLAG(IS_CHROMEOS_ASH)

// A convenience method to create OriginTrialsMap. Note, we only support simple
// cases for chrome:// and chrome-untrusted:// URLs. We don't support complex
// cases such as about:blank (which inherits origins from the embedding frame).
url::Origin GetOrigin(const char* url);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A class for configuring SWAs for all the out-of-application stuff. For
// example, window decorations and initial size. Clients will add a subclass for
// their application, overriding GetWebAppInfo(), and other methods as needed.
class SystemWebAppDelegate {
 public:
  // When installing via a WebApplicationInfo, the url is never loaded. It's
  // needed only for various legacy reasons, maps for tracking state, and
  // generating the AppId and things of that nature.
  SystemWebAppDelegate(
      const SystemAppType type,
      const std::string& internal_name,
      const GURL& install_url,
      Profile* profile,
      const OriginTrialsMap& origin_trials_map = OriginTrialsMap());

  SystemWebAppDelegate(const SystemWebAppDelegate& other) = delete;
  virtual ~SystemWebAppDelegate();

  SystemAppType GetType() const { return type_; }

  // A developer-friendly name for, among other things, reporting metrics
  // and interacting with tast tests. It should follow PascalCase
  // convention, and have a corresponding entry in
  // WebAppSystemAppInternalName histogram suffixes. The internal name
  // shouldn't be changed afterwards.
  const std::string& GetInternalName() const { return internal_name_; }

  // The URL that the System App will be installed from.
  const GURL& GetInstallUrl() const { return install_url_; }

  // Returns a WebApplicationInfo struct to complete installation.
  virtual std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const = 0;

  // If specified, the apps in |uninstall_and_replace| will have their data
  // migrated to this System App.
  virtual std::vector<AppId> GetAppIdsToUninstallAndReplace() const;

  // Minimum window size in DIPs. Empty if the app does not have a minimum.
  // TODO(https://github.com/w3c/manifest/issues/436): Replace with PWA manifest
  // properties for window size.
  virtual gfx::Size GetMinimumWindowSize() const;

  // If set, we allow only a single window for this app.
  virtual bool ShouldBeSingleWindow() const;

  // If true, adds a "New Window" option to App's shelf context menu.
  // ShouldBeSingleWindow() should return false at the same time.
  virtual bool ShouldShowNewWindowMenuOption() const;

  // If true, when the app is launched through the File Handling Web API, we
  // will include the file's directory in window.launchQueue as the first value.
  virtual bool ShouldIncludeLaunchDirectory() const;

  // Map from origin to enabled origin trial names for this app. For example,
  // "chrome://sample-web-app/" to ["Frobulate"]. If set, we will enable the
  // given origin trials when the corresponding origin is loaded in the app.
  const OriginTrialsMap& GetEnabledOriginTrials() const;

  // Resource Ids for additional search terms.
  virtual std::vector<int> GetAdditionalSearchTerms() const;

  // If false, this app will be hidden from the Chrome OS app launcher.
  virtual bool ShouldShowInLauncher() const;

  // If false, this app will be hidden from the Chrome OS search.
  virtual bool ShouldShowInSearch() const;

  // If true, navigations (e.g. Omnibox URL, anchor link) to this app
  // will open in the app's window instead of the navigation's context (e.g.
  // browser tab).
  virtual bool ShouldCaptureNavigations() const;

  // If false, the app will non-resizeable.
  virtual bool ShouldAllowResize() const;

  // If false, the surface of app will can be non-maximizable.
  virtual bool ShouldAllowMaximize() const;

  // If true, the App's window will have a tab-strip.
  virtual bool ShouldHaveTabStrip() const;

  // If false, the app will not have the reload button in minimal ui
  // mode.
  virtual bool ShouldHaveReloadButtonInMinimalUi() const;

  // If true, allows the app to close the window through scripts, for example
  // using `window.close()`.
  virtual bool ShouldAllowScriptsToCloseWindows() const;

  // Setup information to drive a background task.
  virtual absl::optional<SystemAppBackgroundTaskInfo> GetTimerInfo() const;

  // Default window bounds of the application.
  virtual gfx::Rect GetDefaultBounds(Browser* browser) const;

  // If false, the application will not be installed.
  virtual bool IsAppEnabled() const;

 protected:
  SystemAppType type_;
  std::string internal_name_;
  GURL install_url_;
  const Profile* profile_;
  OriginTrialsMap origin_trials_map_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APPS_SYSTEM_WEB_APP_DELEGATE_H_
