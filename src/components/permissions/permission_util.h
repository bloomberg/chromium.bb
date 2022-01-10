// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_UTIL_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_UTIL_H_

#include <string>

#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request.h"

namespace content {
enum class PermissionType;
class WebContents;
class RenderFrameHost;
}  // namespace content

class GURL;

namespace permissions {

// This enum backs a UMA histogram, so it must be treated as append-only.
enum class PermissionAction {
  GRANTED = 0,
  DENIED = 1,
  DISMISSED = 2,
  IGNORED = 3,
  REVOKED = 4,
  GRANTED_ONCE = 5,

  // Always keep this at the end.
  NUM,
};

// A utility class for permissions.
class PermissionUtil {
 public:
  PermissionUtil() = delete;
  PermissionUtil(const PermissionUtil&) = delete;
  PermissionUtil& operator=(const PermissionUtil&) = delete;

  // Returns the permission string for the given permission.
  static std::string GetPermissionString(ContentSettingsType);

  // Returns the gesture type corresponding to whether a permission request is
  // made with or without a user gesture.
  static PermissionRequestGestureType GetGestureType(bool user_gesture);

  // Limited conversion of ContentSettingsType to PermissionType. Returns true
  // if the conversion was performed.
  // TODO(timloh): Try to remove this function. Mainly we need to work out how
  // to remove the usage in PermissionUmaUtil, which uses PermissionType as a
  // histogram value to count permission request metrics.
  static bool GetPermissionType(ContentSettingsType type,
                                content::PermissionType* out);

  // Checks whether the given ContentSettingsType is a permission. Use this
  // to determine whether a specific ContentSettingsType is supported by the
  // PermissionManager.
  static bool IsPermission(ContentSettingsType type);

  // Checks whether the given ContentSettingsType is a guard content setting,
  // meaning it does not support allow setting and toggles between "ask" and
  // "block" instead. This is primarily used for chooser-based permissions.
  static bool IsGuardContentSetting(ContentSettingsType type);

  // Checks whether the given ContentSettingsType supports one time grants.
  static bool CanPermissionBeAllowedOnce(ContentSettingsType type);

  // Returns the authoritative `embedding origin`, as a GURL, to be used for
  // permission decisions in `web_contents`.
  // TODO(crbug.com/698985): This method should only be used temporarily, and
  // ultimately all call sites should be migrated to determine the authoritative
  // security origin based on the requesting RenderFrameHost.
  static GURL GetLastCommittedOriginAsURL(content::WebContents* web_contents);
  static GURL GetLastCommittedOriginAsURL(
      content::RenderFrameHost* render_frame_host);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_UTIL_H_
