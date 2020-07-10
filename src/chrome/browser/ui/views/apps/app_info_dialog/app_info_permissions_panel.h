// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_PANEL_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "extensions/common/permissions/permission_message_provider.h"

class Profile;

namespace extensions {
class Extension;
}

// The summary panel of the app info dialog, which provides basic information
// and controls related to the app.
class AppInfoPermissionsPanel : public AppInfoPanel {
 public:
  AppInfoPermissionsPanel(Profile* profile, const extensions::Extension* app);

  ~AppInfoPermissionsPanel() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           NoPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           RequiredPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           OptionalPermissionsObtainedCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AppInfoPermissionsPanelTest,
                           RetainedFilePermissionsObtainedCorrectly);

  // Called in this order, these methods set-up, add permissions to, and layout
  // the list of permissions.
  void CreatePermissionsList();
  void FillPermissionsList();
  void LayoutPermissionsList();

  bool HasActivePermissionMessages() const;
  extensions::PermissionMessages GetActivePermissionMessages() const;

  int GetRetainedFileCount() const;
  base::string16 GetRetainedFileHeading() const;
  const std::vector<base::string16> GetRetainedFilePaths() const;
  void RevokeFilePermissions();

  int GetRetainedDeviceCount() const;
  base::string16 GetRetainedDeviceHeading() const;
  const std::vector<base::string16> GetRetainedDevices() const;
  void RevokeDevicePermissions();

  DISALLOW_COPY_AND_ASSIGN(AppInfoPermissionsPanel);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_PERMISSIONS_PANEL_H_
