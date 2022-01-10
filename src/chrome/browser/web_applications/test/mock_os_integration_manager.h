// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_OS_INTEGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_OS_INTEGRATION_MANAGER_H_

#include <memory>

#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace web_app {

class WebAppProtocolHandlerManager;

class MockOsIntegrationManager : public OsIntegrationManager {
 public:
  MockOsIntegrationManager();
  explicit MockOsIntegrationManager(
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager);
  ~MockOsIntegrationManager() override;

  // Installation:
  MOCK_METHOD(void,
              CreateShortcuts,
              (const AppId& app_id,
               bool add_to_desktop,
               CreateShortcutsCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterFileHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterProtocolHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              RegisterUrlHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              RegisterShortcutsMenu,
              (const AppId& app_id,
               const std::vector<WebAppShortcutsMenuItemInfo>&
                   shortcuts_menu_item_infos,
               const ShortcutsMenuIconBitmaps& shortcuts_menu_icon_bitmaps,
               ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              ReadAllShortcutsMenuIconsAndRegisterShortcutsMenu,
              (const AppId& app_id, ResultCallback callback),
              (override));

  MOCK_METHOD(void,
              RegisterRunOnOsLogin,
              (const AppId& app_id, RegisterRunOnOsLoginCallback callback),
              (override));

  MOCK_METHOD(void,
              MacAppShimOnAppInstalledForProfile,
              (const AppId& app_id),
              (override));

  MOCK_METHOD(void, AddAppToQuickLaunchBar, (const AppId& app_id), (override));

  MOCK_METHOD(void,
              RegisterWebAppOsUninstallation,
              (const AppId& app_id, const std::string& name),
              (override));

  // Uninstallation:
  MOCK_METHOD(void,
              UninstallAllOsHooks,
              (const AppId& app_id, UninstallOsHooksCallback callback),
              (override));
  MOCK_METHOD(bool, UnregisterShortcutsMenu, (const AppId& app_id), (override));
  MOCK_METHOD(void,
              UnregisterRunOnOsLogin,
              (const AppId& app_id,
               const base::FilePath& profile_path,
               const std::u16string& shortcut_title,
               UnregisterRunOnOsLoginCallback callback),
              (override));
  MOCK_METHOD(void,
              DeleteShortcuts,
              (const AppId& app_id,
               const base::FilePath& shortcuts_data_dir,
               std::unique_ptr<ShortcutInfo> shortcut_info,
               ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              UnregisterFileHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void,
              UnregisterProtocolHandlers,
              (const AppId& app_id, ResultCallback callback),
              (override));
  MOCK_METHOD(void, UnregisterUrlHandlers, (const AppId& app_id), (override));
  MOCK_METHOD(void,
              UnregisterWebAppOsUninstallation,
              (const AppId& app_id),
              (override));

  // Update:
  MOCK_METHOD(void,
              UpdateFileHandlers,
              (const AppId& app_id,
               FileHandlerUpdateAction file_handlers_need_os_update,
               base::OnceClosure finished_callback),
              (override));
  MOCK_METHOD(void,
              UpdateShortcuts,
              (const AppId& app_id,
               base::StringPiece old_name,
               base::OnceClosure callback),
              (override));
  MOCK_METHOD(void,
              UpdateShortcutsMenu,
              (const AppId& app_id, const WebApplicationInfo& web_app_info),
              (override));
  MOCK_METHOD(void,
              UpdateUrlHandlers,
              (const AppId& app_id,
               base::OnceCallback<void(bool success)> callback),
              (override));
  MOCK_METHOD(void,
              UpdateProtocolHandlers,
              (const AppId& app_id,
               bool force_shortcut_updates_if_needed,
               base::OnceClosure update_finished_callback),
              (override));

  // Utility methods:
  MOCK_METHOD(std::unique_ptr<ShortcutInfo>,
              BuildShortcutInfo,
              (const AppId& app_id),
              (override));
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_MOCK_OS_INTEGRATION_MANAGER_H_
