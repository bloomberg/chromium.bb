// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_

#include "base/callback.h"
#include "chrome/browser/installable/installed_webapp_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

class GURL;

class InstalledWebappBridge {
 public:
  using PermissionResponseCallback =
      base::OnceCallback<void(ContentSetting, bool)>;

  InstalledWebappBridge() = delete;
  InstalledWebappBridge(const InstalledWebappBridge&) = delete;
  InstalledWebappBridge& operator=(const InstalledWebappBridge&) = delete;

  static InstalledWebappProvider::RuleList GetInstalledWebappPermissions(
      ContentSettingsType content_type);

  static void SetProviderInstance(InstalledWebappProvider* provider);

  static void DecidePermission(const GURL& origin_url,
                               PermissionResponseCallback callback);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLED_WEBAPP_BRIDGE_H_
