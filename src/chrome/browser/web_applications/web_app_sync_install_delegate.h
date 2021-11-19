// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"

namespace web_app {

class WebApp;

// WebAppSyncBridge delegates sync-initiated installs and uninstalls using
// this interface.
class SyncInstallDelegate {
 public:
  virtual ~SyncInstallDelegate() = default;

  using RepeatingInstallCallback =
      base::RepeatingCallback<void(const AppId& app_id,
                                   InstallResultCode code)>;
  using RepeatingUninstallCallback =
      base::RepeatingCallback<void(const AppId& app_id, bool uninstalled)>;

  // |web_apps| are already registered and owned by the registrar.
  virtual void InstallWebAppsAfterSync(std::vector<WebApp*> web_apps,
                                       RepeatingInstallCallback callback) = 0;

  // Sync-initiated uninstall.
  // Called before the web apps are removed from the registry by sync. This:
  // * Begins process of uninstalling OS hooks, which initially requires the
  //   registrar to still contain the web app data.
  // * Notifies observers of WebAppWillBeUninstalled.
  // After the app data is fully deleted & os hooks uninstalled:
  // * Notifies observers of WebAppUninstalled.
  // * `callback` is called.
  // The registrar is expected to be synchronously updated after this function
  // call to remove the given `web_apps`.
  virtual void UninstallWithoutRegistryUpdateFromSync(
      const std::vector<AppId>& web_apps,
      RepeatingUninstallCallback callback) = 0;

  // Uninstall the given web app ids that were found on startup as partially
  // uninstalled. `apps_to_uninstall` are in the registrar with
  // `is_uninstalling()` set to true. They are expected to be eventually deleted
  // by this call.
  virtual void RetryIncompleteUninstalls(
      const std::vector<AppId>& apps_to_uninstall) = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_SYNC_INSTALL_DELEGATE_H_
