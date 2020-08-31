// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_
#define CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/permissions/permissions_client.h"

class ChromePermissionsClient : public permissions::PermissionsClient {
 public:
  static ChromePermissionsClient* GetInstance();

  // PermissionsClient:
  HostContentSettingsMap* GetSettingsMap(
      content::BrowserContext* browser_context) override;
  permissions::PermissionDecisionAutoBlocker* GetPermissionDecisionAutoBlocker(
      content::BrowserContext* browser_context) override;
  permissions::PermissionManager* GetPermissionManager(
      content::BrowserContext* browser_context) override;
  permissions::ChooserContextBase* GetChooserContext(
      content::BrowserContext* browser_context,
      ContentSettingsType type) override;
  double GetSiteEngagementScore(content::BrowserContext* browser_context,
                                const GURL& origin) override;
  void AreSitesImportant(
      content::BrowserContext* browser_context,
      std::vector<std::pair<url::Origin, bool>>* urls) override;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  bool IsCookieDeletionDisabled(content::BrowserContext* browser_context,
                                const GURL& origin) override;
#endif
  void GetUkmSourceId(content::BrowserContext* browser_context,
                      const content::WebContents* web_contents,
                      const GURL& requesting_origin,
                      GetUkmSourceIdCallback callback) override;
  permissions::PermissionRequest::IconId GetOverrideIconId(
      ContentSettingsType type) override;
  std::unique_ptr<permissions::NotificationPermissionUiSelector>
  CreateNotificationPermissionUiSelector(
      content::BrowserContext* browser_context) override;
  void OnPromptResolved(content::BrowserContext* browser_context,
                        permissions::PermissionRequestType request_type,
                        permissions::PermissionAction action) override;
  base::Optional<url::Origin> GetAutoApprovalOrigin() override;
  bool CanBypassEmbeddingOriginCheck(const GURL& requesting_origin,
                                     const GURL& embedding_origin) override;
  base::Optional<GURL> OverrideCanonicalOrigin(
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
#if defined(OS_ANDROID)
  bool IsPermissionControlledByDse(content::BrowserContext* browser_context,
                                   ContentSettingsType type,
                                   const url::Origin& origin) override;
  bool ResetPermissionIfControlledByDse(
      content::BrowserContext* browser_context,
      ContentSettingsType type,
      const url::Origin& origin) override;
  infobars::InfoBarManager* GetInfoBarManager(
      content::WebContents* web_contents) override;
  infobars::InfoBar* MaybeCreateInfoBar(
      content::WebContents* web_contents,
      ContentSettingsType type,
      base::WeakPtr<permissions::PermissionPromptAndroid> prompt) override;
  void RepromptForAndroidPermissions(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      PermissionsUpdatedCallback callback) override;
  int MapToJavaDrawableId(int resource_id) override;
#else
  std::unique_ptr<permissions::PermissionPrompt> CreatePrompt(
      content::WebContents* web_contents,
      permissions::PermissionPrompt::Delegate* delegate) override;
#endif

 private:
  friend base::NoDestructor<ChromePermissionsClient>;

  ChromePermissionsClient() = default;

  ChromePermissionsClient(const ChromePermissionsClient&) = delete;
  ChromePermissionsClient& operator=(const ChromePermissionsClient&) = delete;
};

#endif  // CHROME_BROWSER_PERMISSIONS_CHROME_PERMISSIONS_CLIENT_H_
