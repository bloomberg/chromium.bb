// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/remote_playback_controller.h"

#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-blink.h"

namespace blink {

VideoWakeLock::VideoWakeLock(HTMLVideoElement& video)
    : PageVisibilityObserver(video.GetDocument().GetPage()),
      video_element_(video) {
  VideoElement().addEventListener(event_type_names::kPlaying, this, true);
  VideoElement().addEventListener(event_type_names::kPause, this, true);
  VideoElement().addEventListener(event_type_names::kEnterpictureinpicture,
                                  this, true);
  VideoElement().addEventListener(event_type_names::kLeavepictureinpicture,
                                  this, true);

  RemotePlaybackController* remote_playback_controller =
      RemotePlaybackController::From(VideoElement());
  if (remote_playback_controller)
    remote_playback_controller->AddObserver(this);
}

void VideoWakeLock::PageVisibilityChanged() {
  Update();
}

void VideoWakeLock::Trace(Visitor* visitor) {
  NativeEventListener::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  visitor->Trace(video_element_);
}

void VideoWakeLock::Invoke(ExecutionContext*, Event* event) {
  if (event->type() == event_type_names::kPlaying) {
    playing_ = true;
  } else if (event->type() == event_type_names::kPause) {
    playing_ = false;
  } else {
    DCHECK(event->type() == event_type_names::kEnterpictureinpicture ||
           event->type() == event_type_names::kLeavepictureinpicture);
  }

  Update();
}

void VideoWakeLock::OnRemotePlaybackStateChanged(WebRemotePlaybackState state) {
  remote_playback_state_ = state;
  Update();
}

void VideoWakeLock::OnRemotePlaybackAvailabilityChanged(
    WebRemotePlaybackAvailability) {}

void VideoWakeLock::Update() {
  bool should_be_active = ShouldBeActive();
  if (should_be_active == active_)
    return;

  active_ = should_be_active;
  UpdateWakeLockService();
}

bool VideoWakeLock::ShouldBeActive() const {
  bool page_visible = GetPage() && GetPage()->IsPageVisible();
  bool in_picture_in_picture =
      PictureInPictureController::IsElementInPictureInPicture(&VideoElement());
  return playing_ && (page_visible || in_picture_in_picture) &&
         remote_playback_state_ != WebRemotePlaybackState::kConnected;
}

void VideoWakeLock::EnsureWakeLockService() {
  if (wake_lock_service_ && wake_lock_service_.is_bound())
    return;

  LocalFrame* frame = VideoElement().GetDocument().GetFrame();
  if (!frame)
    return;

  blink::mojom::blink::WakeLockServicePtr service;
  frame->GetInterfaceProvider().GetInterface(mojo::MakeRequest(&service));
  service->GetWakeLock(device::mojom::WakeLockType::kPreventDisplaySleep,
                       device::mojom::blink::WakeLockReason::kVideoPlayback,
                       "Video Wake Lock",
                       mojo::MakeRequest(&wake_lock_service_));
  wake_lock_service_.set_connection_error_handler(
      WTF::Bind(&VideoWakeLock::OnConnectionError, WrapWeakPersistent(this)));
}

void VideoWakeLock::OnConnectionError() {
  wake_lock_service_.reset();
}

void VideoWakeLock::UpdateWakeLockService() {
  EnsureWakeLockService();

  if (!wake_lock_service_)
    return;

  if (active_)
    wake_lock_service_->RequestWakeLock();
  else
    wake_lock_service_->CancelWakeLock();
}

}  // namespace blink
