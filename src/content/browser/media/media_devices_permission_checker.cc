// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_permission_checker.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/permissions/permission_util.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

MediaDevicesManager::BoolDeviceTypes DoCheckPermissionsOnUIThread(
    MediaDevicesManager::BoolDeviceTypes requested_device_types,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);

  // If there is no |frame_host|, return false for all permissions.
  if (!frame_host)
    return MediaDevicesManager::BoolDeviceTypes();

  RenderFrameHostDelegate* delegate = frame_host->delegate();
  url::Origin origin = frame_host->GetLastCommittedOrigin();
  bool audio_permission = delegate->CheckMediaAccessPermission(
      frame_host, origin, blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  bool mic_permissions_policy = true;
  bool camera_permissions_policy = true;
  mic_permissions_policy = frame_host->IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature::kMicrophone);
  camera_permissions_policy = frame_host->IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature::kCamera);

  MediaDevicesManager::BoolDeviceTypes result;
  // Speakers.
  // TODO(guidou): use specific permission for audio output when it becomes
  // available. See http://crbug.com/556542.
  result[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_OUTPUT)] =
      requested_device_types[static_cast<size_t>(
          MediaDeviceType::MEDIA_AUDIO_OUTPUT)] &&
      audio_permission;

  // Mic.
  result[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_INPUT)] =
      requested_device_types[static_cast<size_t>(
          MediaDeviceType::MEDIA_AUDIO_INPUT)] &&
      audio_permission && mic_permissions_policy;

  // Camera.
  result[static_cast<size_t>(MediaDeviceType::MEDIA_VIDEO_INPUT)] =
      requested_device_types[static_cast<size_t>(
          MediaDeviceType::MEDIA_VIDEO_INPUT)] &&
      delegate->CheckMediaAccessPermission(
          frame_host, origin,
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) &&
      camera_permissions_policy;

  return result;
}

bool CheckSinglePermissionOnUIThread(MediaDeviceType device_type,
                                     int render_process_id,
                                     int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MediaDevicesManager::BoolDeviceTypes requested;
  requested[static_cast<size_t>(device_type)] = true;
  MediaDevicesManager::BoolDeviceTypes result = DoCheckPermissionsOnUIThread(
      requested, render_process_id, render_frame_id);
  return result[static_cast<size_t>(device_type)];
}

}  // namespace

MediaDevicesPermissionChecker::MediaDevicesPermissionChecker()
    : use_override_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)),
      override_value_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kUseFakeUIForMediaStream) != "deny") {}

MediaDevicesPermissionChecker::MediaDevicesPermissionChecker(
    bool override_value)
    : use_override_(true), override_value_(override_value) {}

bool MediaDevicesPermissionChecker::CheckPermissionOnUIThread(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id) const {
  if (use_override_)
    return override_value_;

  return CheckSinglePermissionOnUIThread(device_type, render_process_id,
                                         render_frame_id);
}

void MediaDevicesPermissionChecker::CheckPermission(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> callback) const {
  if (use_override_) {
    std::move(callback).Run(override_value_);
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CheckSinglePermissionOnUIThread, device_type,
                     render_process_id, render_frame_id),
      std::move(callback));
}

void MediaDevicesPermissionChecker::CheckPermissions(
    MediaDevicesManager::BoolDeviceTypes requested,
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(const MediaDevicesManager::BoolDeviceTypes&)>
        callback) const {
  if (use_override_) {
    MediaDevicesManager::BoolDeviceTypes result;
    result.fill(override_value_);
    std::move(callback).Run(result);
    return;
  }

  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DoCheckPermissionsOnUIThread, requested,
                     render_process_id, render_frame_id),
      std::move(callback));
}

// static
bool MediaDevicesPermissionChecker::HasPanTiltZoomPermissionGrantedOnUIThread(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  // The PTZ permission is automatically granted on Android. This way, zoom is
  // not initially empty in ImageCapture. It is safe to do so because pan and
  // tilt are not supported on Android.
  return true;
#else
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);

  if (!frame_host)
    return false;

  auto* web_contents = WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return false;

  auto* permission_controller =
      web_contents->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  // TODO(crbug.com/698985): The semantics of the passed-in origin is incorrect:
  // It should be the requesting origin, not the embedding origin. With the
  // current implementation of the //chrome embedder, this parameter will be
  // ignored, so it has no impact.
  const GURL& origin =
      PermissionUtil::GetLastCommittedOriginAsURL(web_contents);
  blink::mojom::PermissionStatus status =
      permission_controller->GetPermissionStatusForFrame(
          PermissionType::CAMERA_PAN_TILT_ZOOM, frame_host, origin);

  return status == blink::mojom::PermissionStatus::GRANTED;
#endif
}
}  // namespace content
