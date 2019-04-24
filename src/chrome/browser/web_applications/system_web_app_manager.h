// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

// An enum that lists the different System Apps that exist. Can be used to
// retrieve the App ID from the underlying Web App system.
enum class SystemAppType {
  SETTINGS,
  DISCOVER,
};

// Installs, uninstalls, and updates System Web Apps.
class SystemWebAppManager {
 public:
  // Constructs a SystemWebAppManager instance that uses
  // |pending_app_manager| to manage apps. |pending_app_manager| should outlive
  // this class.
  SystemWebAppManager(Profile* profile, PendingAppManager* pending_app_manager);
  virtual ~SystemWebAppManager();

  void Start();

  static bool IsEnabled();

  // Returns the app id for the given System App |id|.
  base::Optional<std::string> GetAppIdForSystemApp(SystemAppType id) const;

 protected:
  void SetSystemAppsForTesting(
      base::flat_map<SystemAppType, GURL> system_app_urls);

 private:
  void StartAppInstallation();

  base::flat_map<SystemAppType, GURL> system_app_urls_;

  // Used to install, uninstall, and update apps. Should outlive this class.
  PendingAppManager* pending_app_manager_;

  base::WeakPtrFactory<SystemWebAppManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_SYSTEM_WEB_APP_MANAGER_H_
