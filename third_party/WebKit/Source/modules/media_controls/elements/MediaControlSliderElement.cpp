// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlSliderElement.h"

#include "core/InputTypeNames.h"
#include "core/dom/ElementShadow.h"
#include "core/html/HTMLDivElement.h"
#include "platform/wtf/text/StringBuilder.h"

namespace {

void SetSegmentDivPosition(blink::HTMLDivElement* segment,
                           int left,
                           int width) {
  StringBuilder builder;
  builder.Append("width: ");
  builder.AppendNumber(width);
  builder.Append("%; left: ");
  builder.AppendNumber(left);
  builder.Append("%;");
  segment->setAttribute("style", builder.ToAtomicString());
}

}  // namespace.

namespace blink {

MediaControlSliderElement::MediaControlSliderElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : MediaControlInputElement(media_controls, display_type),
      segment_highlight_before_(nullptr),
      segment_highlight_after_(nullptr) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::range);
  setAttribute(HTMLNames::stepAttr, "any");
}

void MediaControlSliderElement::SetupBarSegments() {
  DCHECK((segment_highlight_after_ && segment_highlight_before_) ||
         (!segment_highlight_after_ && !segment_highlight_before_));

  if (segment_highlight_after_ || segment_highlight_before_)
    return;

  // The timeline element has a shadow root with the following
  // structure:
  //
  // #shadow-root
  //   - div
  //     - div::-webkit-slider-runnable-track#track
  ShadowRoot& shadow_root = Shadow()->OldestShadowRoot();
  Element* track = shadow_root.getElementById(AtomicString("track"));
  DCHECK(track);
  track->setAttribute("class", "-internal-media-controls-segmented-track");

  // Add the following structure to #track.
  //
  // div.-internal-track-segment-background (container)
  //   - div.-internal-track-segment-highlight-before (blue highlight)
  //   - div.-internal-track-segment-highlight-after (dark gray highlight)
  HTMLDivElement* background = HTMLDivElement::Create(GetDocument());
  background->setAttribute("class", "-internal-track-segment-background");
  track->appendChild(background);

  segment_highlight_before_ = HTMLDivElement::Create(GetDocument());
  segment_highlight_before_->setAttribute(
      "class", "-internal-track-segment-highlight-before");
  background->appendChild(segment_highlight_before_);

  segment_highlight_after_ = HTMLDivElement::Create(GetDocument());
  segment_highlight_after_->setAttribute(
      "class", "-internal-track-segment-highlight-after");
  background->appendChild(segment_highlight_after_);
}

void MediaControlSliderElement::SetBeforeSegmentPosition(int left, int width) {
  DCHECK(segment_highlight_before_);
  SetSegmentDivPosition(segment_highlight_before_, left, width);
}

void MediaControlSliderElement::SetAfterSegmentPosition(int left, int width) {
  DCHECK(segment_highlight_after_);
  SetSegmentDivPosition(segment_highlight_after_, left, width);
}

DEFINE_TRACE(MediaControlSliderElement) {
  visitor->Trace(segment_highlight_before_);
  visitor->Trace(segment_highlight_after_);
  MediaControlInputElement::Trace(visitor);
}

}  // namespace blink
