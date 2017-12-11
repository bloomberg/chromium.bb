// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlOverlayPlayButtonElement.h"

#include "core/dom/ElementShadow.h"
#include "core/dom/events/Event.h"
#include "core/events/MouseEvent.h"
#include "core/geometry/DOMRect.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/media/HTMLMediaSource.h"
#include "core/input_type_names.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/Time.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebSize.h"

namespace {

// The size of the inner circle button in pixels.
constexpr int kInnerButtonSize = 56;

// The touch padding of the inner circle button in pixels.
constexpr int kInnerButtonTouchPaddingSize = 20;

// Check if a point is based within the boundary of a DOMRect with a margin.
bool IsPointInRect(blink::DOMRect& rect, int margin, int x, int y) {
  return ((x >= (rect.left() - margin)) && (x <= (rect.right() + margin)) &&
          (y >= (rect.top() - margin)) && (y <= (rect.bottom() + margin)));
}

// The delay if a touch is outside the internal button.
constexpr WTF::TimeDelta kOutsideTouchDelay = TimeDelta::FromMilliseconds(300);

// The delay if a touch is inside the internal button.
constexpr WTF::TimeDelta kInsideTouchDelay = TimeDelta::FromMilliseconds(0);

// The number of seconds to jump when double tapping.
constexpr int kNumberOfSecondsToJump = 10;

}  // namespace.

namespace blink {

// The DOM structure looks like:
//
// MediaControlOverlayPlayButtonElement
//   (-webkit-media-controls-overlay-play-button)
// +-div (-internal-media-controls-overlay-play-button-internal)
//   {if MediaControlsImpl::IsModern}
//   This contains the inner circle with the actual play/pause icon.
MediaControlOverlayPlayButtonElement::MediaControlOverlayPlayButtonElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaOverlayPlayButton),
      tap_timer_(GetDocument().GetTaskRunner(TaskType::kMediaElementEvent),
                 this,
                 &MediaControlOverlayPlayButtonElement::TapTimerFired),
      internal_button_(nullptr) {
  setType(InputTypeNames::button);
  SetShadowPseudoId(AtomicString("-webkit-media-controls-overlay-play-button"));

  if (MediaControlsImpl::IsModern()) {
    ShadowRoot& shadow_root = Shadow()->OldestShadowRoot();
    internal_button_ = MediaControlElementsHelper::CreateDiv(
        "-internal-media-controls-overlay-play-button-internal", &shadow_root);
  }
}

void MediaControlOverlayPlayButtonElement::UpdateDisplayType() {
  SetIsWanted(MediaElement().ShouldShowControls() &&
              (MediaControlsImpl::IsModern() || MediaElement().paused()));
  MediaControlInputElement::UpdateDisplayType();
}

const char* MediaControlOverlayPlayButtonElement::GetNameForHistograms() const {
  return "PlayOverlayButton";
}

void MediaControlOverlayPlayButtonElement::MaybePlayPause() {
  if (MediaElement().paused()) {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.PlayOverlay"));
  } else {
    Platform::Current()->RecordAction(
        UserMetricsAction("Media.Controls.PauseOverlay"));
  }

  // Allow play attempts for plain src= media to force a reload in the error
  // state. This allows potential recovery for transient network and decoder
  // resource issues.
  const String& url = MediaElement().currentSrc().GetString();
  if (MediaElement().error() && !HTMLMediaElement::IsMediaStreamURL(url) &&
      !HTMLMediaSource::Lookup(url)) {
    MediaElement().load();
  }

  MediaElement().TogglePlayState();

  MaybeRecordInteracted();
  UpdateDisplayType();
}

void MediaControlOverlayPlayButtonElement::MaybeJump(int seconds) {
  double new_time = std::max(0.0, MediaElement().currentTime() + seconds);
  new_time = std::min(new_time, MediaElement().duration());
  MediaElement().setCurrentTime(new_time);
}

void MediaControlOverlayPlayButtonElement::HandlePlayPauseEvent(
    Event* event,
    WTF::TimeDelta delay) {
  event->SetDefaultHandled();

  if (tap_timer_.IsActive())
    return;

  tap_timer_.StartOneShot(delay, BLINK_FROM_HERE);
}

void MediaControlOverlayPlayButtonElement::DefaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    // Double tap to navigate should only be available on modern controls.
    if (!MediaControlsImpl::IsModern() || !event->IsMouseEvent()) {
      HandlePlayPauseEvent(event, kInsideTouchDelay);
      return;
    }

    // If the event doesn't have position data we should just default to
    // play/pause.
    // TODO(beccahughes): Move to PointerEvent.
    MouseEvent* mouse_event = ToMouseEvent(event);
    if (!mouse_event->HasPosition()) {
      HandlePlayPauseEvent(event, kInsideTouchDelay);
      return;
    }

    // If the click happened on the internal button or a margin around it then
    // we should play/pause.
    if (IsPointInRect(*internal_button_->getBoundingClientRect(),
                      kInnerButtonTouchPaddingSize, mouse_event->clientX(),
                      mouse_event->clientY())) {
      HandlePlayPauseEvent(event, kInsideTouchDelay);
    } else if (!tap_timer_.IsActive()) {
      // If there was not a previous touch and this was outside of the button
      // then we should play/pause but with a small unnoticeable delay to allow
      // for a secondary tap.
      HandlePlayPauseEvent(event, kOutsideTouchDelay);
    } else {
      // Cancel the play pause event.
      tap_timer_.Stop();

      if (RuntimeEnabledFeatures::DoubleTapToJumpOnVideoEnabled()) {
        // Jump forwards or backwards based on the position of the tap.
        WebSize element_size =
            MediaControlElementsHelper::GetSizeOrDefault(*this, WebSize(0, 0));

        if (mouse_event->clientX() >= element_size.width / 2) {
          MaybeJump(kNumberOfSecondsToJump);
        } else {
          MaybeJump(kNumberOfSecondsToJump * -1);
        }
      } else {
        // Enter or exit fullscreen.
        if (MediaElement().IsFullscreen())
          GetMediaControls().ExitFullscreen();
        else
          GetMediaControls().EnterFullscreen();
      }

      event->SetDefaultHandled();
    }
  }

  // TODO(mlamouri): should call MediaControlInputElement::DefaultEventHandler.
}

bool MediaControlOverlayPlayButtonElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEvent(event);
}

WebSize MediaControlOverlayPlayButtonElement::GetSizeOrDefault() const {
  // The size should come from the internal button which actually displays the
  // button.
  return MediaControlElementsHelper::GetSizeOrDefault(
      *internal_button_, WebSize(kInnerButtonSize, kInnerButtonSize));
}

void MediaControlOverlayPlayButtonElement::TapTimerFired(TimerBase*) {
  std::unique_ptr<UserGestureIndicator> user_gesture_scope =
      Frame::NotifyUserActivation(GetDocument().GetFrame(),
                                  UserGestureToken::kNewGesture);

  MaybePlayPause();
}

void MediaControlOverlayPlayButtonElement::Trace(blink::Visitor* visitor) {
  MediaControlInputElement::Trace(visitor);
  visitor->Trace(internal_button_);
}

}  // namespace blink
