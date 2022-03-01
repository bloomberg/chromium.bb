// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/sensor_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"

namespace permissions {

SensorPermissionContext::SensorPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::SENSORS,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

SensorPermissionContext::~SensorPermissionContext() {}

void SensorPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.render_process_id(), id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed)
    content_settings->OnContentAllowed(ContentSettingsType::SENSORS);
  else
    content_settings->OnContentBlocked(ContentSettingsType::SENSORS);
}

bool SensorPermissionContext::IsRestrictedToSecureOrigins() const {
  // This is to allow non-secure origins that use DeviceMotion and
  // DeviceOrientation Event to be able to access sensors that are provided
  // by generic_sensor. The Generic Sensor API is not allowed in non-secure
  // origins and this is enforced by the renderer.
  return false;
}

}  // namespace permissions
