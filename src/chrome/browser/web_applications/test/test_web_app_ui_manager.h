// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_UI_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_UI_MANAGER_H_

#include <map>

#include "chrome/browser/web_applications/components/web_app_ui_manager.h"

namespace web_app {

class TestWebAppUiManager : public WebAppUiManager {
 public:
  TestWebAppUiManager();
  TestWebAppUiManager(const TestWebAppUiManager&) = delete;
  TestWebAppUiManager& operator=(const TestWebAppUiManager&) = delete;
  ~TestWebAppUiManager() override;

  void SetSubsystems(AppRegistryController* app_registry_controller) override;
  void Start() override;
  void Shutdown() override;

  void SetNumWindowsForApp(const AppId& app_id, size_t num_windows_for_app);
  bool DidUninstallAndReplace(const AppId& from_app, const AppId& to_app);

  // WebAppUiManager:
  WebAppUiManagerImpl* AsImpl() override;
  size_t GetNumWindowsForApp(const AppId& app_id) override;
  void NotifyOnAllAppWindowsClosed(const AppId& app_id,
                                   base::OnceClosure callback) override;
  bool UninstallAndReplaceIfExists(const std::vector<AppId>& from_apps,
                                   const AppId& to_app) override;
  bool CanAddAppToQuickLaunchBar() const override;
  void AddAppToQuickLaunchBar(const AppId& app_id) override;
  bool IsInAppWindow(content::WebContents* web_contents,
                     const AppId* app_id) const override;
  void NotifyOnAssociatedAppChanged(content::WebContents* web_contents,
                                    const AppId& previous_app_id,
                                    const AppId& new_app_id) const override {}
  bool CanReparentAppTabToWindow(const AppId& app_id,
                                 bool shortcut_created) const override;
  void ReparentAppTabToWindow(content::WebContents* contents,
                              const AppId& app_id,
                              bool shortcut_created) override;

 private:
  std::map<AppId, size_t> app_id_to_num_windows_map_;
  std::map<AppId, AppId> uninstall_and_replace_map_;

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_WEB_APP_UI_MANAGER_H_
