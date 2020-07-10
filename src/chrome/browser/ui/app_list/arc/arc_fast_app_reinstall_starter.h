// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_FAST_APP_REINSTALL_STARTER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_FAST_APP_REINSTALL_STARTER_H_

#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace arc {

// Helper class that starts Play Fast App Reinstall flow when the Play Store app
// is ready.
class ArcFastAppReinstallStarter : public ArcAppListPrefs::Observer {
 public:
  ArcFastAppReinstallStarter(content::BrowserContext* context,
                             PrefService* pref_service);
  ~ArcFastAppReinstallStarter() override;

  // Creating Fast App Reinstall starter will call MaybeStartFastAppReinstall().
  // If the flow has already started, there is no need to create a new starter.
  static std::unique_ptr<ArcFastAppReinstallStarter> CreateIfNeeded(
      content::BrowserContext* context,
      PrefService* pref_service);

  bool started() const { return started_; }

  void OnAppsSelectionFinished();

 private:
  void MaybeStartFastAppReinstall();

  // ArcAppListPrefs::Observer:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;

  content::BrowserContext* const context_;
  PrefService* const pref_service_;
  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcFastAppReinstallStarter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_FAST_APP_REINSTALL_STARTER_H_
