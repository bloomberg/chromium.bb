// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_permission_manager.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/media_switches.h"

namespace content {

namespace {

bool IsAllowlistedPermissionType(PermissionType permission) {
  switch (permission) {
    case PermissionType::GEOLOCATION:
    case PermissionType::MIDI:
    case PermissionType::SENSORS:
    case PermissionType::ACCESSIBILITY_EVENTS:
    case PermissionType::PAYMENT_HANDLER:
    case PermissionType::WAKE_LOCK_SCREEN:

    // Background Sync and Background Fetch browser tests require
    // permission to be granted by default.
    case PermissionType::BACKGROUND_SYNC:
    case PermissionType::BACKGROUND_FETCH:
    case PermissionType::PERIODIC_BACKGROUND_SYNC:

    case PermissionType::IDLE_DETECTION:

    // Storage Access API web platform tests require permission to be granted by
    // default.
    case PermissionType::STORAGE_ACCESS_GRANT:

    // WebNFC browser tests require permission to be granted by default.
    case PermissionType::NFC:
      return true;

    case PermissionType::MIDI_SYSEX:
    case PermissionType::NOTIFICATIONS:
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::AUDIO_CAPTURE:
    case PermissionType::VIDEO_CAPTURE:
    case PermissionType::CLIPBOARD_READ_WRITE:
    case PermissionType::CLIPBOARD_SANITIZED_WRITE:
    case PermissionType::NUM:
    case PermissionType::WAKE_LOCK_SYSTEM:
    case PermissionType::VR:
    case PermissionType::AR:
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
    case PermissionType::WINDOW_PLACEMENT:
    case PermissionType::FONT_ACCESS:
    case PermissionType::DISPLAY_CAPTURE:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace

ShellPermissionManager::ShellPermissionManager() = default;

ShellPermissionManager::~ShellPermissionManager() {
}

void ShellPermissionManager::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }
  std::move(callback).Run(IsAllowlistedPermissionType(permission)
                              ? blink::mojom::PermissionStatus::GRANTED
                              : blink::mojom::PermissionStatus::DENIED);
}

void ShellPermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(std::vector<blink::mojom::PermissionStatus>(
        permissions.size(), blink::mojom::PermissionStatus::DENIED));
    return;
  }
  std::vector<blink::mojom::PermissionStatus> result;
  for (const auto& permission : permissions) {
    result.push_back(IsAllowlistedPermissionType(permission)
                         ? blink::mojom::PermissionStatus::GRANTED
                         : blink::mojom::PermissionStatus::DENIED);
  }
  std::move(callback).Run(result);
}

void ShellPermissionManager::ResetPermission(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

blink::mojom::PermissionStatus ShellPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if ((permission == PermissionType::AUDIO_CAPTURE ||
       permission == PermissionType::VIDEO_CAPTURE) &&
      command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream) &&
      command_line->HasSwitch(switches::kUseFakeUIForMediaStream)) {
    return blink::mojom::PermissionStatus::GRANTED;
  }

  return IsAllowlistedPermissionType(permission)
             ? blink::mojom::PermissionStatus::GRANTED
             : blink::mojom::PermissionStatus::DENIED;
}

blink::mojom::PermissionStatus
ShellPermissionManager::GetPermissionStatusForFrame(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  if (render_frame_host->IsNestedWithinFencedFrame())
    return blink::mojom::PermissionStatus::DENIED;
  return GetPermissionStatus(
      permission, requesting_origin,
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          content::WebContents::FromRenderFrameHost(render_frame_host)));
}

ShellPermissionManager::SubscriptionId
ShellPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  return SubscriptionId();
}

void ShellPermissionManager::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}

}  // namespace content
