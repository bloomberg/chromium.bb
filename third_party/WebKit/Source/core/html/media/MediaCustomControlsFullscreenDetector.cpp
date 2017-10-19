// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaCustomControlsFullscreenDetector.h"

#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/layout/IntersectionGeometry.h"

namespace blink {

namespace {

constexpr double kCheckFullscreenIntervalSeconds = 1.0f;
constexpr float kMostlyFillViewportThresholdOfOccupationProportion = 0.85f;
constexpr float kMostlyFillViewportThresholdOfVisibleProportion = 0.75f;

}  // anonymous namespace

MediaCustomControlsFullscreenDetector::MediaCustomControlsFullscreenDetector(
    HTMLVideoElement& video)
    : EventListener(kCPPEventListenerType),
      video_element_(video),
      check_viewport_intersection_timer_(
          TaskRunnerHelper::Get(TaskType::kUnthrottled, &video.GetDocument()),
          this,
          &MediaCustomControlsFullscreenDetector::
              OnCheckViewportIntersectionTimerFired) {
  if (VideoElement().isConnected())
    Attach();
}

bool MediaCustomControlsFullscreenDetector::operator==(
    const EventListener& other) const {
  return this == &other;
}

void MediaCustomControlsFullscreenDetector::Attach() {
  VideoElement().addEventListener(EventTypeNames::loadedmetadata, this, true);
  VideoElement().GetDocument().addEventListener(
      EventTypeNames::webkitfullscreenchange, this, true);
  VideoElement().GetDocument().addEventListener(
      EventTypeNames::fullscreenchange, this, true);
}

void MediaCustomControlsFullscreenDetector::Detach() {
  VideoElement().removeEventListener(EventTypeNames::loadedmetadata, this,
                                     true);
  VideoElement().GetDocument().removeEventListener(
      EventTypeNames::webkitfullscreenchange, this, true);
  VideoElement().GetDocument().removeEventListener(
      EventTypeNames::fullscreenchange, this, true);
  check_viewport_intersection_timer_.Stop();

  if (VideoElement().GetWebMediaPlayer())
    VideoElement().GetWebMediaPlayer()->SetIsEffectivelyFullscreen(false);
}

bool MediaCustomControlsFullscreenDetector::ComputeIsDominantVideoForTests(
    const IntRect& target_rect,
    const IntRect& root_rect,
    const IntRect& intersection_rect) {
  if (target_rect.IsEmpty() || root_rect.IsEmpty())
    return false;

  const float x_occupation_proportion =
      1.0f * intersection_rect.Width() / root_rect.Width();
  const float y_occupation_proportion =
      1.0f * intersection_rect.Height() / root_rect.Height();

  // If the viewport is mostly occupied by the video, return true.
  if (std::min(x_occupation_proportion, y_occupation_proportion) >=
      kMostlyFillViewportThresholdOfOccupationProportion) {
    return true;
  }

  // If neither of the dimensions of the viewport is mostly occupied by the
  // video, return false.
  if (std::max(x_occupation_proportion, y_occupation_proportion) <
      kMostlyFillViewportThresholdOfOccupationProportion) {
    return false;
  }

  // If the video is mostly visible in the indominant dimension, return true.
  // Otherwise return false.
  if (x_occupation_proportion > y_occupation_proportion) {
    return target_rect.Height() *
               kMostlyFillViewportThresholdOfVisibleProportion <
           intersection_rect.Height();
  }
  return target_rect.Width() * kMostlyFillViewportThresholdOfVisibleProportion <
         intersection_rect.Width();
}

void MediaCustomControlsFullscreenDetector::handleEvent(
    ExecutionContext* context,
    Event* event) {
  DCHECK(event->type() == EventTypeNames::loadedmetadata ||
         event->type() == EventTypeNames::webkitfullscreenchange ||
         event->type() == EventTypeNames::fullscreenchange);

  // Video is not loaded yet.
  if (VideoElement().getReadyState() < HTMLMediaElement::kHaveMetadata)
    return;

  if (!VideoElement().isConnected() || !IsVideoOrParentFullscreen()) {
    check_viewport_intersection_timer_.Stop();

    if (VideoElement().GetWebMediaPlayer())
      VideoElement().GetWebMediaPlayer()->SetIsEffectivelyFullscreen(false);

    return;
  }

  check_viewport_intersection_timer_.StartOneShot(
      kCheckFullscreenIntervalSeconds, BLINK_FROM_HERE);
}

void MediaCustomControlsFullscreenDetector::ContextDestroyed() {
  // This method is called by HTMLVideoElement when it observes context destroy.
  // The reason is that when HTMLMediaElement observes context destroy, it will
  // destroy webMediaPlayer() thus the final setIsEffectivelyFullscreen(false)
  // is not called.
  Detach();
}

void MediaCustomControlsFullscreenDetector::
    OnCheckViewportIntersectionTimerFired(TimerBase*) {
  DCHECK(IsVideoOrParentFullscreen());
  IntersectionGeometry geometry(nullptr, VideoElement(), Vector<Length>(),
                                true);
  geometry.ComputeGeometry();

  bool is_dominant = ComputeIsDominantVideoForTests(
      geometry.TargetIntRect(), geometry.RootIntRect(),
      geometry.IntersectionIntRect());

  if (VideoElement().GetWebMediaPlayer())
    VideoElement().GetWebMediaPlayer()->SetIsEffectivelyFullscreen(is_dominant);
}

bool MediaCustomControlsFullscreenDetector::IsVideoOrParentFullscreen() {
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(VideoElement().GetDocument());
  if (!fullscreen_element)
    return false;

  return fullscreen_element->contains(&VideoElement());
}

void MediaCustomControlsFullscreenDetector::Trace(blink::Visitor* visitor) {
  EventListener::Trace(visitor);
  visitor->Trace(video_element_);
}

}  // namespace blink
