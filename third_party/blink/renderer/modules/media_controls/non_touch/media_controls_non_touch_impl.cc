// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

#include <algorithm>

#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_media_event_listener.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

// Number of seconds to jump when press left/right arrow.
constexpr int kNumberOfSecondsToJumpForNonTouch = 10;

// Amount of volume to change when press up/down arrow.
constexpr double kVolumeToChangeForNonTouch = 0.05;

}  // namespace

enum class MediaControlsNonTouchImpl::ArrowDirection {
  kUp,
  kDown,
  kLeft,
  kRight,
};

MediaControlsNonTouchImpl::MediaControlsNonTouchImpl(
    HTMLMediaElement& media_element)
    : HTMLDivElement(media_element.GetDocument()),
      MediaControls(media_element),
      media_event_listener_(
          MakeGarbageCollected<MediaControlsNonTouchMediaEventListener>(
              media_element)) {
  SetShadowPseudoId(AtomicString("-internal-media-controls-non-touch"));
  media_event_listener_->AddObserver(this);
}

MediaControlsNonTouchImpl* MediaControlsNonTouchImpl::Create(
    HTMLMediaElement& media_element,
    ShadowRoot& shadow_root) {
  MediaControlsNonTouchImpl* controls =
      MakeGarbageCollected<MediaControlsNonTouchImpl>(media_element);
  shadow_root.ParserAppendChild(controls);
  return controls;
}

Node::InsertionNotificationRequest MediaControlsNonTouchImpl::InsertedInto(
    ContainerNode& root) {
  media_event_listener_->Attach();
  return HTMLDivElement::InsertedInto(root);
}

void MediaControlsNonTouchImpl::RemovedFrom(ContainerNode& insertion_point) {
  HTMLDivElement::RemovedFrom(insertion_point);
  Hide();
  media_event_listener_->Detach();
}

void MediaControlsNonTouchImpl::MaybeShow() {
  // show controls
}

void MediaControlsNonTouchImpl::Hide() {
  // hide controls
}

void MediaControlsNonTouchImpl::OnFocusIn() {
  if (MediaElement().ShouldShowControls())
    MaybeShow();
}

void MediaControlsNonTouchImpl::OnKeyDown(KeyboardEvent* event) {
  bool handled = true;
  switch (event->keyCode()) {
    case VKEY_RETURN:
      MediaElement().TogglePlayState();
      break;
    case VKEY_LEFT:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kLeft));
      break;
    case VKEY_RIGHT:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kRight));
      break;
    case VKEY_UP:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kUp));
      break;
    case VKEY_DOWN:
      HandleOrientedArrowPress(OrientArrowPress(ArrowDirection::kDown));
      break;
    default:
      handled = false;
      break;
  }

  if (handled)
    event->SetDefaultHandled();
}

MediaControlsNonTouchImpl::ArrowDirection
MediaControlsNonTouchImpl::OrientArrowPress(ArrowDirection direction) {
  switch (GetOrientation()) {
    case kWebScreenOrientationUndefined:
    case kWebScreenOrientationPortraitPrimary:
      return direction;
    case kWebScreenOrientationPortraitSecondary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kDown;
        case ArrowDirection::kDown:
          return ArrowDirection::kUp;
        case ArrowDirection::kLeft:
          return ArrowDirection::kRight;
        case ArrowDirection::kRight:
          return ArrowDirection::kLeft;
      }
    case kWebScreenOrientationLandscapePrimary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kLeft;
        case ArrowDirection::kDown:
          return ArrowDirection::kRight;
        case ArrowDirection::kLeft:
          return ArrowDirection::kDown;
        case ArrowDirection::kRight:
          return ArrowDirection::kUp;
      }
    case kWebScreenOrientationLandscapeSecondary:
      switch (direction) {
        case ArrowDirection::kUp:
          return ArrowDirection::kRight;
        case ArrowDirection::kDown:
          return ArrowDirection::kLeft;
        case ArrowDirection::kLeft:
          return ArrowDirection::kUp;
        case ArrowDirection::kRight:
          return ArrowDirection::kDown;
      }
  }
}

void MediaControlsNonTouchImpl::HandleOrientedArrowPress(
    ArrowDirection direction) {
  switch (direction) {
    case ArrowDirection::kUp:
      HandleTopButtonPress();
      break;
    case ArrowDirection::kDown:
      HandleBottomButtonPress();
      break;
    case ArrowDirection::kLeft:
      HandleLeftButtonPress();
      break;
    case ArrowDirection::kRight:
      HandleRightButtonPress();
      break;
  }
}

WebScreenOrientationType MediaControlsNonTouchImpl::GetOrientation() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return kWebScreenOrientationUndefined;

  return frame->GetChromeClient().GetScreenInfo().orientation_type;
}

void MediaControlsNonTouchImpl::HandleTopButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForNonTouch);
}

void MediaControlsNonTouchImpl::HandleBottomButtonPress() {
  MaybeChangeVolume(kVolumeToChangeForNonTouch * -1);
}

void MediaControlsNonTouchImpl::HandleLeftButtonPress() {
  MaybeJump(kNumberOfSecondsToJumpForNonTouch * -1);
}

void MediaControlsNonTouchImpl::HandleRightButtonPress() {
  MaybeJump(kNumberOfSecondsToJumpForNonTouch);
}

void MediaControlsNonTouchImpl::MaybeChangeVolume(double volume_to_change) {
  double new_volume = std::max(0.0, MediaElement().volume() + volume_to_change);
  new_volume = std::min(new_volume, 1.0);
  MediaElement().setVolume(new_volume);
}

void MediaControlsNonTouchImpl::MaybeJump(int seconds) {
  double new_time = std::max(0.0, MediaElement().currentTime() + seconds);
  new_time = std::min(new_time, MediaElement().duration());
  MediaElement().setCurrentTime(new_time);
}

void MediaControlsNonTouchImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_event_listener_);
  MediaControls::Trace(visitor);
  HTMLDivElement::Trace(visitor);
}

}  // namespace blink
