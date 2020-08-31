// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_TEST_PERMISSIONS_CLIENT_H_
#define COMPONENTS_PERMISSIONS_TEST_TEST_PERMISSIONS_CLIENT_H_

#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permissions_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace permissions {

// PermissionsClient to be used in tests which implements all pure virtual
// methods. This class assumes only one BrowserContext will be used.
class TestPermissionsClient : public PermissionsClient {
 public:
  TestPermissionsClient();
  ~TestPermissionsClient() override;

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  PermissionManager* GetPermissionManager(
      content::BrowserContext* browser_context) override;
  ChooserContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;
  void GetUkmSourceId(content::BrowserContext* browser_context,
                      const content::WebContents* web_contents,
                      const GURL& requesting_origin,
                      GetUkmSourceIdCallback callback) override;

 private:
  TestPermissionsClient(const TestPermissionsClient&) = delete;
  TestPermissionsClient& operator=(const TestPermissionsClient&) = delete;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  scoped_refptr<HostContentSettingsMap> settings_map_;
  PermissionDecisionAutoBlocker autoblocker_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_TEST_PERMISSIONS_CLIENT_H_
