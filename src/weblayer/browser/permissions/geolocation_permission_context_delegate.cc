// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/geolocation_permission_context_delegate.h"

#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "weblayer/browser/android/permission_request_utils.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/tab_impl.h"
#endif

namespace weblayer {

bool GeolocationPermissionContextDelegate::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback* callback,
    permissions::GeolocationPermissionContext* context) {
  return false;
}

#if defined(OS_ANDROID)
bool GeolocationPermissionContextDelegate::IsInteractable(
    content::WebContents* web_contents) {
  auto* tab = TabImpl::FromWebContents(web_contents);
  return tab && tab->IsActive();
}

PrefService* GeolocationPermissionContextDelegate::GetPrefs(
    content::BrowserContext* browser_context) {
  return static_cast<BrowserContextImpl*>(browser_context)->pref_service();
}

bool GeolocationPermissionContextDelegate::IsRequestingOriginDSE(
    content::BrowserContext* browser_context,
    const GURL& requesting_origin) {
  // TODO(crbug.com/1063433): This may need to be implemented for phase 3.
  return false;
}

void GeolocationPermissionContextDelegate::FinishNotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}
#endif

}  // namespace weblayer
