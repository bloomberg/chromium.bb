// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsRotateToFullscreenDelegate.h"

#include "core/dom/ElementVisibilityObserver.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/HTMLVideoElement.h"
#include "core/page/ChromeClient.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

namespace {

// Videos must be at least this big in both dimensions to qualify.
constexpr unsigned kMinSize = 200;

// At least this fraction of the video must be visible.
constexpr float kVisibilityThreshold = 0.75;

}  // anonymous namespace

MediaControlsRotateToFullscreenDelegate::
    MediaControlsRotateToFullscreenDelegate(HTMLVideoElement& video)
    : EventListener(kCPPEventListenerType), video_element_(video) {}

void MediaControlsRotateToFullscreenDelegate::Attach() {
  DCHECK(video_element_->isConnected());

  LocalDOMWindow* dom_window = video_element_->GetDocument().domWindow();
  if (!dom_window)
    return;

  video_element_->addEventListener(EventTypeNames::play, this, true);
  video_element_->addEventListener(EventTypeNames::pause, this, true);

  // Listen to two different fullscreen events in order to make sure the new and
  // old APIs are handled.
  video_element_->addEventListener(EventTypeNames::webkitfullscreenchange, this,
                                   true);
  video_element_->GetDocument().addEventListener(
      EventTypeNames::fullscreenchange, this, true);

  current_screen_orientation_ = ComputeScreenOrientation();
  // TODO(johnme): Check this is battery efficient (note that this doesn't need
  // to receive events for 180 deg rotations).
  dom_window->addEventListener(EventTypeNames::orientationchange, this, false);
}

void MediaControlsRotateToFullscreenDelegate::Detach() {
  DCHECK(!video_element_->isConnected());

  if (visibility_observer_) {
    // TODO(johnme): Should I also call Stop in a prefinalizer?
    visibility_observer_->Stop();
    visibility_observer_ = nullptr;
    is_visible_ = false;
  }

  video_element_->removeEventListener(EventTypeNames::play, this, true);
  video_element_->removeEventListener(EventTypeNames::pause, this, true);

  video_element_->removeEventListener(EventTypeNames::webkitfullscreenchange,
                                      this, true);
  video_element_->GetDocument().removeEventListener(
      EventTypeNames::fullscreenchange, this, true);

  LocalDOMWindow* dom_window = video_element_->GetDocument().domWindow();
  if (!dom_window)
    return;
  dom_window->removeEventListener(EventTypeNames::orientationchange, this,
                                  false);
}

bool MediaControlsRotateToFullscreenDelegate::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaControlsRotateToFullscreenDelegate::handleEvent(
    ExecutionContext* execution_context,
    Event* event) {
  if (event->type() == EventTypeNames::play ||
      event->type() == EventTypeNames::pause ||
      event->type() == EventTypeNames::fullscreenchange ||
      event->type() == EventTypeNames::webkitfullscreenchange) {
    OnStateChange();
    return;
  }
  if (event->type() == EventTypeNames::orientationchange) {
    OnScreenOrientationChange();
    return;
  }

  NOTREACHED();
}

void MediaControlsRotateToFullscreenDelegate::OnStateChange() {
  // TODO(johnme): Check this aggressive disabling doesn't lead to race
  // conditions where we briefly don't know if the video is visible.
  bool needs_visibility_observer =
      !video_element_->paused() && !video_element_->IsFullscreen();
  DVLOG(3) << __func__ << " " << !!visibility_observer_ << " -> "
           << needs_visibility_observer;

  if (needs_visibility_observer && !visibility_observer_) {
    visibility_observer_ = new ElementVisibilityObserver(
        video_element_,
        WTF::Bind(&MediaControlsRotateToFullscreenDelegate::OnVisibilityChange,
                  WrapWeakPersistent(this)));
    visibility_observer_->Start(kVisibilityThreshold);
  } else if (!needs_visibility_observer && visibility_observer_) {
    visibility_observer_->Stop();
    visibility_observer_ = nullptr;
    is_visible_ = false;
  }
}

void MediaControlsRotateToFullscreenDelegate::OnVisibilityChange(
    bool is_visible) {
  DVLOG(3) << __func__ << " " << is_visible_ << " -> " << is_visible;
  is_visible_ = is_visible;
}

void MediaControlsRotateToFullscreenDelegate::OnScreenOrientationChange() {
  SimpleOrientation previous_screen_orientation = current_screen_orientation_;
  current_screen_orientation_ = ComputeScreenOrientation();
  DVLOG(3) << __func__ << " " << static_cast<int>(previous_screen_orientation)
           << " -> " << static_cast<int>(current_screen_orientation_);

  // Only enable if native media controls are used.
  if (!video_element_->ShouldShowControls())
    return;

  // Don't enter/exit fullscreen if some other element is fullscreen.
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(video_element_->GetDocument());
  if (fullscreen_element && fullscreen_element != video_element_)
    return;

  // To enter fullscreen, video must be visible and playing.
  // TODO(johnme): If orientation changes whilst this tab is in the background,
  // we'll get an orientationchange event when this tab next becomes active.
  // Check that those events don't trigger rotate-to-fullscreen.
  if (!video_element_->IsFullscreen() &&
      (!is_visible_ || video_element_->paused())) {
    return;
  }

  // Ignore (unexpected) events where we have incomplete information.
  if (previous_screen_orientation == SimpleOrientation::kUnknown ||
      current_screen_orientation_ == SimpleOrientation::kUnknown) {
    return;
  }

  // Ignore 180 degree rotations between PortraitPrimary and PortraitSecondary,
  // or between LandscapePrimary and LandscapeSecondary.
  if (previous_screen_orientation == current_screen_orientation_)
    return;

  SimpleOrientation video_orientation = ComputeVideoOrientation();

  // Ignore videos that are too small or of unknown size.
  if (video_orientation == SimpleOrientation::kUnknown)
    return;

  MediaControlsImpl& media_controls =
      *static_cast<MediaControlsImpl*>(video_element_->GetMediaControls());

  {
    UserGestureIndicator gesture(
        UserGestureToken::Create(&video_element_->GetDocument()));

    bool should_be_fullscreen =
        current_screen_orientation_ == video_orientation;
    if (should_be_fullscreen && !video_element_->IsFullscreen()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Video.RotateToFullscreen.Enter"));
      media_controls.EnterFullscreen();
    } else if (!should_be_fullscreen && video_element_->IsFullscreen()) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Video.RotateToFullscreen.Exit"));
      media_controls.ExitFullscreen();
    }
  }
}

MediaControlsRotateToFullscreenDelegate::SimpleOrientation
MediaControlsRotateToFullscreenDelegate::ComputeVideoOrientation() const {
  if (video_element_->getReadyState() == HTMLMediaElement::kHaveNothing)
    return SimpleOrientation::kUnknown;

  const unsigned width = video_element_->videoWidth();
  const unsigned height = video_element_->videoHeight();

  if (width < kMinSize || height < kMinSize)
    return SimpleOrientation::kUnknown;  // Too small, ignore this video.

  if (width >= height)
    return SimpleOrientation::kLandscape;  // Includes square videos.
  return SimpleOrientation::kPortrait;
}

MediaControlsRotateToFullscreenDelegate::SimpleOrientation
MediaControlsRotateToFullscreenDelegate::ComputeScreenOrientation() const {
  Frame* frame = video_element_->GetDocument().GetFrame();
  if (!frame)
    return SimpleOrientation::kUnknown;

  switch (frame->GetChromeClient().GetScreenInfo().orientation_type) {
    case kWebScreenOrientationPortraitPrimary:
    case kWebScreenOrientationPortraitSecondary:
      return SimpleOrientation::kPortrait;
    case kWebScreenOrientationLandscapePrimary:
    case kWebScreenOrientationLandscapeSecondary:
      return SimpleOrientation::kLandscape;
    case kWebScreenOrientationUndefined:
      return SimpleOrientation::kUnknown;
  }

  NOTREACHED();
  return SimpleOrientation::kUnknown;
}

DEFINE_TRACE(MediaControlsRotateToFullscreenDelegate) {
  EventListener::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(visibility_observer_);
}

}  // namespace blink
