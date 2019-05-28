// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/elements/media_controls_touchless_timeline_element.h"

#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/modules/media_controls/elements/media_control_elements_helper.h"
#include "third_party/blink/renderer/modules/media_controls/media_controls_shared_helper.h"
#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

MediaControlsTouchlessTimelineElement::MediaControlsTouchlessTimelineElement(
    MediaControlsTouchlessImpl& media_controls)
    : MediaControlsTouchlessElement(media_controls) {
  SetShadowPseudoId(
      AtomicString("-internal-media-controls-touchless-timeline"));

  loaded_bar_ = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-timeline-loaded", this);
  progress_bar_ = MediaControlElementsHelper::CreateDiv(
      "-internal-media-controls-touchless-timeline-progress", loaded_bar_);
}

void MediaControlsTouchlessTimelineElement::OnLoadingProgress() {
  UpdateBarsCSS();
}

void MediaControlsTouchlessTimelineElement::OnTimeUpdate() {
  current_time_ = MediaElement().currentTime();
  UpdateBars();
}

void MediaControlsTouchlessTimelineElement::OnSeeking() {
  current_time_ = MediaElement().currentTime();
  UpdateBars();
}

void MediaControlsTouchlessTimelineElement::OnDurationChange() {
  duration_ = MediaElement().duration();
  UpdateBars();
}

void MediaControlsTouchlessTimelineElement::UpdateBars() {
  if (std::isnan(duration_) || std::isinf(duration_) || !duration_ ||
      std::isnan(current_time_)) {
    progress_percent_ = 0;
    loaded_percent_ = 0;
    UpdateBarsCSS();
    return;
  }

  progress_percent_ = current_time_ / duration_;
  loaded_percent_ = progress_percent_;

  base::Optional<unsigned> current_buffered_time_range =
      MediaControlsSharedHelpers::GetCurrentBufferedTimeRange(MediaElement());
  if (current_buffered_time_range) {
    TimeRanges* buffered_time_ranges = MediaElement().buffered();
    float end = buffered_time_ranges->end(current_buffered_time_range.value(),
                                          ASSERT_NO_EXCEPTION);
    loaded_percent_ = end / duration_;
  }

  UpdateBarsCSS();
}

void MediaControlsTouchlessTimelineElement::UpdateBarsCSS() {
  SetBarWidth(loaded_bar_, loaded_percent_);

  // Since progress bar is a child of loaded bar, we need to calculate
  // the percentage accordingly.
  double adjusted_width_percent = 0;
  if (loaded_percent_ != 0)
    adjusted_width_percent = progress_percent_ / loaded_percent_;

  SetBarWidth(progress_bar_, adjusted_width_percent);
}

void MediaControlsTouchlessTimelineElement::SetBarWidth(HTMLDivElement* bar,
                                                        double percent) {
  StringBuilder builder;
  builder.Append("width:");
  builder.AppendNumber(percent * 100);
  builder.Append("%");
  bar->setAttribute("style", builder.ToAtomicString());
}

void MediaControlsTouchlessTimelineElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(loaded_bar_);
  visitor->Trace(progress_bar_);
  MediaControlsTouchlessElement::Trace(visitor);
}

}  // namespace blink
