// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlSliderElement.h"

#include "core/dom/ElementShadow.h"
#include "core/html/HTMLDivElement.h"
#include "core/input_type_names.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/LayoutView.h"
#include "core/resize_observer/ResizeObserver.h"
#include "core/resize_observer/ResizeObserverEntry.h"
#include "platform/wtf/text/StringBuilder.h"

namespace {

void SetSegmentDivPosition(blink::HTMLDivElement* segment,
                           blink::MediaControlSliderElement::Position position,
                           int width,
                           float zoom_factor) {
  int segment_width = int((position.width * width) / zoom_factor);
  int segment_left = int((position.left * width) / zoom_factor);

  StringBuilder builder;
  builder.Append("width: ");
  builder.AppendNumber(segment_width);
  builder.Append("px; left: ");
  builder.AppendNumber(segment_left);
  builder.Append("px;");
  segment->setAttribute("style", builder.ToAtomicString());
}

}  // namespace.

namespace blink {

class MediaControlSliderElement::MediaControlSliderElementResizeObserverDelegate
    final : public ResizeObserver::Delegate {
 public:
  explicit MediaControlSliderElementResizeObserverDelegate(
      MediaControlSliderElement* element)
      : element_(element) {
    DCHECK(element);
  }
  ~MediaControlSliderElementResizeObserverDelegate() override = default;

  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());
    DCHECK_EQ(entries[0]->target(), element_);
    element_->NotifyElementSizeChanged();
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(element_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<MediaControlSliderElement> element_;
};

MediaControlSliderElement::MediaControlSliderElement(
    MediaControlsImpl& media_controls,
    MediaControlElementType display_type)
    : MediaControlInputElement(media_controls, display_type),
      before_segment_position_(0, 0),
      after_segment_position_(0, 0),
      segment_highlight_before_(nullptr),
      segment_highlight_after_(nullptr),
      resize_observer_(ResizeObserver::Create(
          GetDocument(),
          new MediaControlSliderElementResizeObserverDelegate(this))) {
  EnsureUserAgentShadowRoot();
  setType(InputTypeNames::range);
  setAttribute(HTMLNames::stepAttr, "any");
  resize_observer_->observe(this);
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
  track->SetShadowPseudoId("-internal-media-controls-segmented-track");

  // Add the following structure to #track.
  //
  // div::internal-track-segment-background (container)
  //   - div::internal-track-segment-highlight-before (blue highlight)
  //   - div::internal-track-segment-highlight-after (dark gray highlight)
  HTMLDivElement* background = HTMLDivElement::Create(GetDocument());
  background->SetShadowPseudoId("-internal-track-segment-background");
  track->appendChild(background);

  segment_highlight_before_ = HTMLDivElement::Create(GetDocument());
  segment_highlight_before_->SetShadowPseudoId(
      "-internal-track-segment-highlight-before");
  background->appendChild(segment_highlight_before_);

  segment_highlight_after_ = HTMLDivElement::Create(GetDocument());
  segment_highlight_after_->SetShadowPseudoId(
      "-internal-track-segment-highlight-after");
  background->appendChild(segment_highlight_after_);
}

void MediaControlSliderElement::SetBeforeSegmentPosition(
    MediaControlSliderElement::Position position) {
  DCHECK(segment_highlight_before_);
  before_segment_position_ = position;
  SetSegmentDivPosition(segment_highlight_before_, before_segment_position_,
                        Width(), ZoomFactor());
}

void MediaControlSliderElement::SetAfterSegmentPosition(
    MediaControlSliderElement::Position position) {
  DCHECK(segment_highlight_after_);
  after_segment_position_ = position;
  SetSegmentDivPosition(segment_highlight_after_, after_segment_position_,
                        Width(), ZoomFactor());
}

int MediaControlSliderElement::Width() {
  if (LayoutBoxModelObject* box = GetLayoutBoxModelObject())
    return box->OffsetWidth().Round();
  return 0;
}

float MediaControlSliderElement::ZoomFactor() const {
  if (!GetDocument().GetLayoutView())
    return 1;
  return GetDocument().GetLayoutView()->ZoomFactor();
}

void MediaControlSliderElement::NotifyElementSizeChanged() {
  SetSegmentDivPosition(segment_highlight_before_, before_segment_position_,
                        Width(), ZoomFactor());
  SetSegmentDivPosition(segment_highlight_after_, after_segment_position_,
                        Width(), ZoomFactor());
}

void MediaControlSliderElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(segment_highlight_before_);
  visitor->Trace(segment_highlight_after_);
  visitor->Trace(resize_observer_);
  MediaControlInputElement::Trace(visitor);
}

}  // namespace blink
