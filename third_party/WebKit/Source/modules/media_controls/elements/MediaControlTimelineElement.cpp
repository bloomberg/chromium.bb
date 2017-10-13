// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlTimelineElement.h"

#include "core/dom/ShadowRoot.h"
#include "core/dom/events/Event.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/PointerEvent.h"
#include "core/html/TimeRanges.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html_names.h"
#include "core/input_type_names.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/page/ChromeClient.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScreenInfo.h"

namespace {

const double kCurrentTimeBufferedDelta = 1.0;

}  // namespace.

namespace blink {

MediaControlTimelineElement::MediaControlTimelineElement(
    MediaControlsImpl& media_controls)
    : MediaControlSliderElement(media_controls, kMediaSlider) {
  SetShadowPseudoId(AtomicString("-webkit-media-controls-timeline"));
}

bool MediaControlTimelineElement::WillRespondToMouseClickEvents() {
  return isConnected() && GetDocument().IsActive();
}

void MediaControlTimelineElement::SetPosition(double current_time) {
  setValue(String::Number(current_time));
  RenderBarSegments();
}

void MediaControlTimelineElement::SetDuration(double duration) {
  SetFloatingPointAttribute(HTMLNames::maxAttr,
                            std::isfinite(duration) ? duration : 0);
  RenderBarSegments();
}

void MediaControlTimelineElement::OnPlaying() {
  Frame* frame = GetDocument().GetFrame();
  if (!frame)
    return;
  metrics_.RecordPlaying(
      frame->GetChromeClient().GetScreenInfo().orientation_type,
      MediaElement().IsFullscreen(), Width());
}

const char* MediaControlTimelineElement::GetNameForHistograms() const {
  return "TimelineSlider";
}

void MediaControlTimelineElement::DefaultEventHandler(Event* event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

  RenderBarSegments();

  // Only respond to main button of primary pointer(s).
  if (event->IsPointerEvent() && ToPointerEvent(event)->isPrimary() &&
      ToPointerEvent(event)->button() ==
          static_cast<short>(WebPointerProperties::Button::kLeft)) {
    if (event->type() == EventTypeNames::pointerdown) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ScrubbingBegin"));
      GetMediaControls().BeginScrubbing();
      Element* thumb = UserAgentShadowRoot()->getElementById(
          ShadowElementNames::SliderThumb());
      bool started_from_thumb = thumb && thumb == event->target()->ToNode();
      metrics_.StartGesture(started_from_thumb);
    }
    if (event->type() == EventTypeNames::pointerup) {
      Platform::Current()->RecordAction(
          UserMetricsAction("Media.Controls.ScrubbingEnd"));
      GetMediaControls().EndScrubbing();
      metrics_.RecordEndGesture(Width(), MediaElement().duration());
    }
  }

  if (event->type() == EventTypeNames::keydown) {
    metrics_.StartKey();
  }
  if (event->type() == EventTypeNames::keyup && event->IsKeyboardEvent()) {
    metrics_.RecordEndKey(Width(), ToKeyboardEvent(event)->keyCode());
  }

  MediaControlInputElement::DefaultEventHandler(event);

  if (event->IsMouseEvent() || event->IsKeyboardEvent() ||
      event->IsGestureEvent() || event->IsPointerEvent()) {
    MaybeRecordInteracted();
  }

  if (event->type() != EventTypeNames::input)
    return;

  double time = value().ToDouble();

  double duration = MediaElement().duration();
  // Workaround for floating point error - it's possible for this element's max
  // attribute to be rounded to a value slightly higher than the duration. If
  // this happens and scrubber is dragged near the max, seek to duration.
  if (time > duration)
    time = duration;

  metrics_.OnInput(MediaElement().currentTime(), time);

  // FIXME: This will need to take the timeline offset into consideration
  // once that concept is supported, see https://crbug.com/312699
  if (MediaElement().seekable()->Contain(time))
    MediaElement().setCurrentTime(time);

  // Provide immediate feedback (without waiting for media to seek) to make it
  // easier for user to seek to a precise time.
  GetMediaControls().UpdateCurrentTimeDisplay();
}

bool MediaControlTimelineElement::KeepEventInNode(Event* event) {
  return MediaControlElementsHelper::IsUserInteractionEventForSlider(
      event, GetLayoutObject());
}

void MediaControlTimelineElement::RenderBarSegments() {
  SetupBarSegments();

  double current_time = MediaElement().currentTime();
  double duration = MediaElement().duration();

  // Draw the buffered range. Since the element may have multiple buffered
  // ranges and it'd be distracting/'busy' to show all of them, show only the
  // buffered range containing the current play head.
  TimeRanges* buffered_time_ranges = MediaElement().buffered();
  DCHECK(buffered_time_ranges);
  if (std::isnan(duration) || std::isinf(duration) || !duration ||
      std::isnan(current_time)) {
    SetBeforeSegmentPosition(MediaControlSliderElement::Position(0, 0));
    SetAfterSegmentPosition(MediaControlSliderElement::Position(0, 0));
    return;
  }

  // int current_position = int(current_time * Width() / duration);
  double current_position = current_time / duration;
  for (unsigned i = 0; i < buffered_time_ranges->length(); ++i) {
    float start = buffered_time_ranges->start(i, ASSERT_NO_EXCEPTION);
    float end = buffered_time_ranges->end(i, ASSERT_NO_EXCEPTION);
    // The delta is there to avoid corner cases when buffered
    // ranges is out of sync with current time because of
    // asynchronous media pipeline and current time caching in
    // HTMLMediaElement.
    // This is related to https://www.w3.org/Bugs/Public/show_bug.cgi?id=28125
    // FIXME: Remove this workaround when WebMediaPlayer
    // has an asynchronous pause interface.
    if (std::isnan(start) || std::isnan(end) ||
        start > current_time + kCurrentTimeBufferedDelta ||
        end < current_time) {
      continue;
    }

    // int start_position = int(start * Width() / duration);
    // int end_position = int(end * Width() / duration);
    double start_position = start / duration;
    double end_position = end / duration;

    // Draw highlight to show what we have played.
    if (current_position > start_position) {
      SetAfterSegmentPosition(MediaControlSliderElement::Position(
          start_position, current_position));
    }

    // Draw dark grey highlight to show what we have loaded.
    if (end_position > current_position) {
      SetBeforeSegmentPosition(MediaControlSliderElement::Position(
          current_position, end_position - current_position));
    }
    return;
  }

  // Reset the widths to hide the segments.
  SetBeforeSegmentPosition(MediaControlSliderElement::Position(0, 0));
  SetAfterSegmentPosition(MediaControlSliderElement::Position(0, 0));
}

}  // namespace blink
