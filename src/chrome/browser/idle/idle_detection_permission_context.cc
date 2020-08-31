// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle/idle_detection_permission_context.h"

#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "url/gurl.h"

IdleDetectionPermissionContext::IdleDetectionPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::IDLE_DETECTION,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

IdleDetectionPermissionContext::~IdleDetectionPermissionContext() = default;

void IdleDetectionPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  content_settings::TabSpecificContentSettings* content_settings =
      content_settings::TabSpecificContentSettings::GetForFrame(
          id.render_process_id(), id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed)
    content_settings->OnContentAllowed(ContentSettingsType::IDLE_DETECTION);
  else
    content_settings->OnContentBlocked(ContentSettingsType::IDLE_DETECTION);
}

ContentSetting IdleDetectionPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ALLOW;
}

bool IdleDetectionPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
