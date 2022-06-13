// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_JOB_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "url/origin.h"

class PrefService;

namespace webapps {
enum class WebappUninstallSource;
}

namespace web_app {

class OsIntegrationManager;
class WebAppIconManager;
class WebAppRegistrar;
class WebAppSyncBridge;

enum class WebAppUninstallJobResult {
  kSuccess = 0,
  kError = 1,
};

// Uninstalls a given web app by:
// 1) Unregistering OS hooks.
// 2) Deleting the app from the database.
// 3) Deleting data on disk.
// Extra invariants:
// * There is never more than one uninstall task operating on the same app at
//   the same time.
// TODO(https://crbug.com/1162477): Make the database delete happen last.
class WebAppUninstallJob {
 public:
  using UninstallCallback = base::OnceCallback<void(WebAppUninstallJobResult)>;

  WebAppUninstallJob(OsIntegrationManager* os_integration_manager,
                     WebAppSyncBridge* sync_bridge,
                     WebAppIconManager* icon_manager,
                     WebAppRegistrar* registrar,
                     PrefService* profile_prefs);
  ~WebAppUninstallJob();

  enum class ModifyAppRegistry {
    // Modify the app to set `is_uninstalling()` to true, and delete the app
    // from the registry after uninstallation is complete. Used by non-sync
    // uninstall.
    kYes,
    // Do not modify the app in the registry or delete the app from the
    // registry.
    kNo
  };
  // The given `app_id` must correspond to an app in the `registrar`.
  void Start(const AppId& app_id,
             url::Origin app_origin,
             webapps::WebappUninstallSource source,
             ModifyAppRegistry delete_option,
             UninstallCallback callback);

  // If a sync uninstall is triggered while a regular uninstall is occurring,
  // change the deletion option to `ModifyAppRegistry::kNo`, as the registry
  // will be modified by sync instead.
  void StopAppRegistryModification();

 private:
  void OnOsHooksUninstalled(OsHooksErrors errors);
  void OnIconDataDeleted(bool success);
  void MaybeFinishUninstall();

  enum class State {
    kNotStarted = 0,
    kPendingDataDeletion = 1,
    kDone = 2,
  } state_ = State::kNotStarted;

  raw_ptr<OsIntegrationManager> os_integration_manager_;
  raw_ptr<WebAppSyncBridge> sync_bridge_;
  raw_ptr<WebAppIconManager> icon_manager_;
  raw_ptr<WebAppRegistrar> registrar_;
  raw_ptr<PrefService> profile_prefs_;

  AppId app_id_;
  webapps::WebappUninstallSource source_;
  ModifyAppRegistry delete_option_;
  UninstallCallback callback_;

  bool app_data_deleted_ = false;
  bool hooks_uninstalled_ = false;
  bool errors_ = false;

  base::WeakPtrFactory<WebAppUninstallJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_UNINSTALL_JOB_H_
