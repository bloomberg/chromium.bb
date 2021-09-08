// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"
#include "content/public/browser/browser_context.h"

class GeolocationPermissionContextDelegateAndroid
    : public GeolocationPermissionContextDelegate {
 public:
  explicit GeolocationPermissionContextDelegateAndroid(Profile* profile);
  ~GeolocationPermissionContextDelegateAndroid() override;

  // GeolocationPermissionContext::Delegate:
  bool DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback* callback,
      permissions::GeolocationPermissionContext* context) override;

  bool IsInteractable(content::WebContents* web_contents) override;
  PrefService* GetPrefs(content::BrowserContext* browser_context) override;
  bool IsRequestingOriginDSE(content::BrowserContext* browser_context,
                             const GURL& requesting_origin) override;
  void FinishNotifyPermissionSet(const permissions::PermissionRequestID& id,
                                 const GURL& requesting_origin,
                                 const GURL& embedding_origin) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContextDelegateAndroid);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_DELEGATE_ANDROID_H_
