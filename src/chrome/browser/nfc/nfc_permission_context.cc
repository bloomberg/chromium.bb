// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/nfc_permission_context.h"

#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"

NfcPermissionContext::NfcPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::NFC,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

NfcPermissionContext::~NfcPermissionContext() = default;

#if !defined(OS_ANDROID)
ContentSetting NfcPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_BLOCK;
}
#endif

void NfcPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  if (!user_gesture) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }
  permissions::PermissionContextBase::DecidePermission(
      web_contents, id, requesting_origin, embedding_origin, user_gesture,
      std::move(callback));
}

void NfcPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  auto* content_settings =
      content_settings::TabSpecificContentSettings::GetForFrame(
          id.render_process_id(), id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed)
    content_settings->OnContentAllowed(ContentSettingsType::NFC);
  else
    content_settings->OnContentBlocked(ContentSettingsType::NFC);
}

bool NfcPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
