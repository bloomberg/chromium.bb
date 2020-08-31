// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/image_capture/image_capture_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {

namespace {

void GetPhotoStateOnIOThread(const std::string& source_id,
                             MediaStreamManager* media_stream_manager,
                             ImageCaptureImpl::GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::UnguessableToken session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);
  if (session_id.is_empty())
    return;

  media_stream_manager->video_capture_manager()->GetPhotoState(
      session_id, std::move(callback));
}

void SetOptionsOnIOThread(const std::string& source_id,
                          MediaStreamManager* media_stream_manager,
                          media::mojom::PhotoSettingsPtr settings,
                          ImageCaptureImpl::SetOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const base::UnguessableToken session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);
  if (session_id.is_empty())
    return;
  media_stream_manager->video_capture_manager()->SetPhotoOptions(
      session_id, std::move(settings), std::move(callback));
}

void TakePhotoOnIOThread(const std::string& source_id,
                         MediaStreamManager* media_stream_manager,
                         ImageCaptureImpl::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "image_capture_impl.cc::TakePhotoOnIOThread",
                       TRACE_EVENT_SCOPE_PROCESS);

  const base::UnguessableToken session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);
  if (session_id.is_empty())
    return;

  media_stream_manager->video_capture_manager()->TakePhoto(session_id,
                                                           std::move(callback));
}

}  // anonymous namespace

// static
void ImageCaptureImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::ImageCapture> receiver) {
  DCHECK(render_frame_host);
  // ImageCaptureImpl owns itself. It will self-destruct when a Mojo interface
  // error occurs, the render frame host is deleted, or the render frame host
  // navigates to a new document.
  new ImageCaptureImpl(render_frame_host, std::move(receiver));
}

void ImageCaptureImpl::GetPhotoState(const std::string& source_id,
                                     GetPhotoStateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::GetPhotoState",
                       TRACE_EVENT_SCOPE_PROCESS);

  GetPhotoStateCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(
              base::BindOnce(&ImageCaptureImpl::OnGetPhotoState,
                             weak_factory_.GetWeakPtr(), std::move(callback))),
          mojo::CreateEmptyPhotoState());
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetPhotoStateOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

void ImageCaptureImpl::SetOptions(const std::string& source_id,
                                  media::mojom::PhotoSettingsPtr settings,
                                  SetOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::SetOptions",
                       TRACE_EVENT_SCOPE_PROCESS);

  // TODO(crbug.com/934063): Check "has_zoom" as well if upcoming metrics show
  // that zoom may be moved under this permission.
  if ((settings->has_pan || settings->has_tilt) &&
      !HasPanTiltZoomPermissionGranted()) {
    std::move(callback).Run(false);
    return;
  }

  SetOptionsCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(std::move(callback)), false);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SetOptionsOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(settings), std::move(scoped_callback)));
}

void ImageCaptureImpl::TakePhoto(const std::string& source_id,
                                 TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCaptureImpl::TakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);

  TakePhotoCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(std::move(callback)),
          media::mojom::Blob::New());
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&TakePhotoOnIOThread, source_id,
                     BrowserMainLoop::GetInstance()->media_stream_manager(),
                     std::move(scoped_callback)));
}

ImageCaptureImpl::ImageCaptureImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::ImageCapture> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

ImageCaptureImpl::~ImageCaptureImpl() = default;

void ImageCaptureImpl::OnGetPhotoState(GetPhotoStateCallback callback,
                                       media::mojom::PhotoStatePtr state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/934063): Reset "zoom" as well if upcoming metrics show
  // that zoom may be moved under this permission.
  if (!HasPanTiltZoomPermissionGranted()) {
    state->pan = media::mojom::Range::New();
    state->tilt = media::mojom::Range::New();
  }
  std::move(callback).Run(std::move(state));
}

bool ImageCaptureImpl::HasPanTiltZoomPermissionGranted() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  // Camera PTZ is desktop only at the moment.
  return true;
#else
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    return false;
  }

  auto* web_contents = WebContents::FromRenderFrameHost(render_frame_host());
  auto* permission_controller = BrowserContext::GetPermissionController(
      web_contents->GetBrowserContext());
  DCHECK(permission_controller);

  blink::mojom::PermissionStatus status =
      permission_controller->GetPermissionStatusForFrame(
          PermissionType::CAMERA_PAN_TILT_ZOOM, render_frame_host(),
          origin().GetURL());

  return status == blink::mojom::PermissionStatus::GRANTED;
#endif
}
}  // namespace content
