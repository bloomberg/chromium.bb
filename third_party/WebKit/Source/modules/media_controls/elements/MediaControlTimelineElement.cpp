// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlTimelineElement.h"

#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/events/Event.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/PointerEvent.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/page/ChromeClient.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/media_controls/elements/MediaControlElementsHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

MediaControlTimelineElement::MediaControlTimelineElement(
    MediaControlsImpl& media_controls)
    : MediaControlInputElement(media_controls, kMediaSlider) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::range);
  setAttribute(HTMLNames::stepAttr, "any");
  SetShadowPseudoId(AtomicString("-webkit-media-controls-timeline"));
}

bool MediaControlTimelineElement::WillRespondToMouseClickEvents() {
  return isConnected() && GetDocument().IsActive();
}

void MediaControlTimelineElement::SetPosition(double current_time) {
  setValue(String::Number(current_time));

  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

void MediaControlTimelineElement::SetDuration(double duration) {
  SetFloatingPointAttribute(HTMLNames::maxAttr,
                            std::isfinite(duration) ? duration : 0);

  if (LayoutObject* layout_object = this->GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

void MediaControlTimelineElement::OnPlaying() {
  Frame* frame = GetDocument().GetFrame();
  if (!frame)
    return;
  metrics_.RecordPlaying(
      frame->GetChromeClient().GetScreenInfo().orientation_type,
      MediaElement().IsFullscreen(), TimelineWidth());
}

const char* MediaControlTimelineElement::GetNameForHistograms() const {
  return "TimelineSlider";
}

void MediaControlTimelineElement::DefaultEventHandler(Event* event) {
  if (!isConnected() || !GetDocument().IsActive())
    return;

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
      metrics_.RecordEndGesture(TimelineWidth(), MediaElement().duration());
    }
  }

  if (event->type() == EventTypeNames::keydown) {
    metrics_.StartKey();
  }
  if (event->type() == EventTypeNames::keyup && event->IsKeyboardEvent()) {
    metrics_.RecordEndKey(TimelineWidth(), ToKeyboardEvent(event)->keyCode());
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

int MediaControlTimelineElement::TimelineWidth() {
  if (LayoutBoxModelObject* box = GetLayoutBoxModelObject())
    return box->OffsetWidth().Round();
  return 0;
}

}  // namespace blink
