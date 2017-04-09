// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsOrientationLockDelegate.h"

#include "core/events/Event.h"
#include "core/frame/ScreenOrientationController.h"
#include "core/html/HTMLVideoElement.h"
#include "core/page/ChromeClient.h"
#include "platform/Histogram.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"

namespace blink {

namespace {

// These values are used for histograms. Do not reorder.
enum class MetadataAvailabilityMetrics {
  kAvailable = 0,  // Available when lock was attempted.
  kMissing = 1,    // Missing when lock was attempted.
  kReceived = 2,   // Received after being missing in order to lock.

  // Keep at the end.
  kMax = 3
};

// These values are used for histograms. Do not reorder.
enum class LockResultMetrics {
  kAlreadyLocked = 0,  // Frame already has a lock.
  kPortrait = 1,       // Locked to portrait.
  kLandscape = 2,      // Locked to landscape.

  // Keep at the end.
  kMax = 3
};

void RecordMetadataAvailability(MetadataAvailabilityMetrics metrics) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, metadata_histogram,
      ("Media.Video.FullscreenOrientationLock.MetadataAvailability",
       static_cast<int>(MetadataAvailabilityMetrics::kMax)));
  metadata_histogram.Count(static_cast<int>(metrics));
}

void RecordLockResult(LockResultMetrics metrics) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, lock_result_histogram,
                      ("Media.Video.FullscreenOrientationLock.LockResult",
                       static_cast<int>(LockResultMetrics::kMax)));
  lock_result_histogram.Count(static_cast<int>(metrics));
}

// WebLockOrientationCallback implementation that will not react to a success
// nor a failure.
class DummyScreenOrientationCallback : public WebLockOrientationCallback {
 public:
  void OnSuccess() override {}
  void OnError(WebLockOrientationError) override {}
};

}  // anonymous namespace

MediaControlsOrientationLockDelegate::MediaControlsOrientationLockDelegate(
    HTMLVideoElement& video)
    : EventListener(kCPPEventListenerType), video_element_(video) {
  if (VideoElement().isConnected())
    Attach();
}

void MediaControlsOrientationLockDelegate::Attach() {
  DCHECK(VideoElement().isConnected());

  GetDocument().addEventListener(EventTypeNames::fullscreenchange, this, true);
  VideoElement().addEventListener(EventTypeNames::webkitfullscreenchange, this,
                                  true);
  VideoElement().addEventListener(EventTypeNames::loadedmetadata, this, true);
}

void MediaControlsOrientationLockDelegate::Detach() {
  DCHECK(!VideoElement().isConnected());

  GetDocument().removeEventListener(EventTypeNames::fullscreenchange, this,
                                    true);
  VideoElement().removeEventListener(EventTypeNames::webkitfullscreenchange,
                                     this, true);
  VideoElement().removeEventListener(EventTypeNames::loadedmetadata, this,
                                     true);
}

bool MediaControlsOrientationLockDelegate::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsOrientationLockDelegate::MaybeLockOrientation() {
  DCHECK(state_ != State::kMaybeLockedFullscreen);

  if (VideoElement().getReadyState() == HTMLMediaElement::kHaveNothing) {
    RecordMetadataAvailability(MetadataAvailabilityMetrics::kMissing);
    state_ = State::kPendingMetadata;
    return;
  }

  if (state_ == State::kPendingMetadata)
    RecordMetadataAvailability(MetadataAvailabilityMetrics::kReceived);
  else
    RecordMetadataAvailability(MetadataAvailabilityMetrics::kAvailable);

  state_ = State::kMaybeLockedFullscreen;

  if (!GetDocument().GetFrame())
    return;

  auto controller =
      ScreenOrientationController::From(*GetDocument().GetFrame());
  if (controller->MaybeHasActiveLock()) {
    RecordLockResult(LockResultMetrics::kAlreadyLocked);
    return;
  }

  WebScreenOrientationLockType orientation_lock = ComputeOrientationLock();
  controller->lock(orientation_lock,
                   WTF::WrapUnique(new DummyScreenOrientationCallback));
  should_unlock_orientation_ = true;

  if (orientation_lock == kWebScreenOrientationLockLandscape)
    RecordLockResult(LockResultMetrics::kLandscape);
  else
    RecordLockResult(LockResultMetrics::kPortrait);
}

void MediaControlsOrientationLockDelegate::MaybeUnlockOrientation() {
  DCHECK(state_ != State::kPendingFullscreen);

  state_ = State::kPendingFullscreen;

  if (!should_unlock_orientation_)
    return;

  ScreenOrientationController::From(*GetDocument().GetFrame())->unlock();
  should_unlock_orientation_ = false;
}

HTMLVideoElement& MediaControlsOrientationLockDelegate::VideoElement() const {
  return *video_element_;
}

Document& MediaControlsOrientationLockDelegate::GetDocument() const {
  return VideoElement().GetDocument();
}

void MediaControlsOrientationLockDelegate::handleEvent(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == EventTypeNames::fullscreenchange ||
      event->type() == EventTypeNames::webkitfullscreenchange) {
    if (VideoElement().IsFullscreen()) {
      if (state_ == State::kPendingFullscreen)
        MaybeLockOrientation();
    } else {
      if (state_ != State::kPendingFullscreen)
        MaybeUnlockOrientation();
    }

    return;
  }

  if (event->type() == EventTypeNames::loadedmetadata) {
    if (state_ == State::kPendingMetadata)
      MaybeLockOrientation();

    return;
  }

  NOTREACHED();
}

WebScreenOrientationLockType
MediaControlsOrientationLockDelegate::ComputeOrientationLock() const {
  DCHECK(VideoElement().getReadyState() != HTMLMediaElement::kHaveNothing);

  const unsigned width = VideoElement().videoWidth();
  const unsigned height = VideoElement().videoHeight();

  if (width > height)
    return kWebScreenOrientationLockLandscape;

  if (height > width)
    return kWebScreenOrientationLockPortrait;

  // For square videos, try to lock to the current screen orientation for
  // consistency. Use WebScreenOrientationLockLandscape as a fallback value.
  // TODO(mlamouri): we could improve this by having direct access to
  // `window.screen.orientation.type`.
  Frame* frame = GetDocument().GetFrame();
  if (!frame)
    return kWebScreenOrientationLockLandscape;

  switch (frame->GetChromeClient().GetScreenInfo().orientation_type) {
    case kWebScreenOrientationPortraitPrimary:
    case kWebScreenOrientationPortraitSecondary:
      return kWebScreenOrientationLockPortrait;
    case kWebScreenOrientationLandscapePrimary:
    case kWebScreenOrientationLandscapeSecondary:
      return kWebScreenOrientationLockLandscape;
    case kWebScreenOrientationUndefined:
      return kWebScreenOrientationLockLandscape;
  }

  NOTREACHED();
  return kWebScreenOrientationLockLandscape;
}

DEFINE_TRACE(MediaControlsOrientationLockDelegate) {
  EventListener::Trace(visitor);
  visitor->Trace(video_element_);
}

}  // namespace blink
