// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/weblayer_permissions_client.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/default_search_engine.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "weblayer/browser/permissions/permission_manager_factory.h"

#if defined(OS_ANDROID)
#include "weblayer/browser/android/permission_request_utils.h"
#include "weblayer/browser/android/resource_mapper.h"
#endif

namespace weblayer {

// static
WebLayerPermissionsClient* WebLayerPermissionsClient::GetInstance() {
  static base::NoDestructor<WebLayerPermissionsClient> instance;
  return instance.get();
}

HostContentSettingsMap* WebLayerPermissionsClient::GetSettingsMap(
    content::BrowserContext* browser_context) {
  return HostContentSettingsMapFactory::GetForBrowserContext(browser_context);
}

scoped_refptr<content_settings::CookieSettings>
WebLayerPermissionsClient::GetCookieSettings(
    content::BrowserContext* browser_context) {
  return CookieSettingsFactory::GetForBrowserContext(browser_context);
}

bool WebLayerPermissionsClient::IsSubresourceFilterActivated(
    content::BrowserContext* browser_context,
    const GURL& url) {
  // As the web layer does not currently support subresource filtering, the
  // activation setting does not change any browser behavior.
  return false;
}

permissions::PermissionDecisionAutoBlocker*
WebLayerPermissionsClient::GetPermissionDecisionAutoBlocker(
    content::BrowserContext* browser_context) {
  return PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
      browser_context);
}

permissions::PermissionManager* WebLayerPermissionsClient::GetPermissionManager(
    content::BrowserContext* browser_context) {
  return PermissionManagerFactory::GetForBrowserContext(browser_context);
}

permissions::ChooserContextBase* WebLayerPermissionsClient::GetChooserContext(
    content::BrowserContext* browser_context,
    ContentSettingsType type) {
  return nullptr;
}

#if defined(OS_ANDROID)
bool WebLayerPermissionsClient::IsPermissionControlledByDse(
    content::BrowserContext* browser_context,
    ContentSettingsType type,
    const url::Origin& origin) {
  return weblayer::IsPermissionControlledByDse(type, origin);
}

bool WebLayerPermissionsClient::ResetPermissionIfControlledByDse(
    content::BrowserContext* browser_context,
    ContentSettingsType type,
    const url::Origin& origin) {
  if (IsPermissionControlledByDse(browser_context, type, origin)) {
    ResetDsePermissions(browser_context);
    return true;
  }
  return false;
}

void WebLayerPermissionsClient::RepromptForAndroidPermissions(
    content::WebContents* web_contents,
    const std::vector<ContentSettingsType>& content_settings_types,
    PermissionsUpdatedCallback callback) {
  RequestAndroidPermissions(web_contents, content_settings_types,
                            std::move(callback));
}

int WebLayerPermissionsClient::MapToJavaDrawableId(int resource_id) {
  return weblayer::MapToJavaDrawableId(resource_id);
}
#endif

}  // namespace weblayer
