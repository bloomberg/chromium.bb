/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_slider_container.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/slider_thumb_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/layout_slider.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

LayoutSliderContainer::LayoutSliderContainer(SliderContainerElement* element)
    : LayoutFlexibleBox(element) {}

inline static Decimal SliderPosition(HTMLInputElement* element) {
  const StepRange step_range(element->CreateStepRange(kRejectAny));
  const Decimal old_value =
      ParseToDecimalForNumberType(element->value(), step_range.DefaultValue());
  return step_range.ProportionFromValue(step_range.ClampValue(old_value));
}

void LayoutSliderContainer::ComputeLogicalHeight(
    LayoutUnit logical_height,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  auto* input = To<HTMLInputElement>(GetNode()->OwnerShadowHost());

  if (input->GetLayoutObject()->IsSlider() && input->list()) {
    int offset_from_center =
        LayoutTheme::GetTheme().SliderTickOffsetFromTrackCenter();
    LayoutUnit track_height;
    if (offset_from_center < 0) {
      track_height = LayoutUnit(-2 * offset_from_center);
    } else {
      int tick_length = LayoutTheme::GetTheme().SliderTickSize().Height();
      track_height = LayoutUnit(2 * (offset_from_center + tick_length));
    }
    float zoom_factor = StyleRef().EffectiveZoom();
    if (zoom_factor != 1.0)
      track_height *= zoom_factor;

    // FIXME: The trackHeight should have been added before updateLogicalHeight
    // was called to avoid this hack.
    SetIntrinsicContentLogicalHeight(track_height);

    LayoutBox::ComputeLogicalHeight(track_height, logical_top, computed_values);
    return;
  }

  // FIXME: The trackHeight should have been added before updateLogicalHeight
  // was called to avoid this hack.
  SetIntrinsicContentLogicalHeight(logical_height);

  LayoutBox::ComputeLogicalHeight(logical_height, logical_top, computed_values);
}

MinMaxSizes LayoutSliderContainer::ComputeIntrinsicLogicalWidths() const {
  MinMaxSizes sizes;
  sizes += LayoutUnit(LayoutSlider::kDefaultTrackLength *
                      StyleRef().EffectiveZoom()) +
           BorderAndPaddingLogicalWidth();
  return sizes;
}

void LayoutSliderContainer::UpdateLayout() {
  auto* input = To<HTMLInputElement>(GetNode()->OwnerShadowHost());
  const bool is_vertical = !StyleRef().IsHorizontalWritingMode();

  Element* thumb_element = input->UserAgentShadowRoot()->getElementById(
      shadow_element_names::SliderThumb());
  Element* track_element = input->UserAgentShadowRoot()->getElementById(
      shadow_element_names::SliderTrack());
  LayoutBox* thumb = thumb_element ? thumb_element->GetLayoutBox() : nullptr;
  LayoutBox* track = track_element ? track_element->GetLayoutBox() : nullptr;

  SubtreeLayoutScope layout_scope(*this);
  // Force a layout to reset the position of the thumb so the code below doesn't
  // move the thumb to the wrong place.
  // FIXME: Make a custom layout class for the track and move the thumb
  // positioning code there.
  if (track)
    layout_scope.SetChildNeedsLayout(track);

  LayoutFlexibleBox::UpdateLayout();

  // These should always exist, unless someone mutates the shadow DOM (e.g., in
  // the inspector).
  if (!thumb || !track)
    return;

  double percentage_offset = SliderPosition(input).ToDouble();
  LayoutUnit available_extent =
      is_vertical ? track->ContentHeight() : track->ContentWidth();
  available_extent -=
      is_vertical ? thumb->Size().Height() : thumb->Size().Width();
  LayoutUnit offset(percentage_offset * available_extent);
  LayoutPoint thumb_location = thumb->Location();
  if (is_vertical) {
    thumb_location.SetY(thumb_location.Y() - offset);
  } else if (StyleRef().IsLeftToRightDirection()) {
    thumb_location.SetX(thumb_location.X() + offset);
  } else {
    thumb_location.SetX(thumb_location.X() - offset);
  }
  thumb->SetLocation(thumb_location);

  // We need one-off invalidation code here because painting of the timeline
  // element does not go through style.
  // Instead it has a custom implementation in C++ code.
  // Therefore the style system cannot understand when it needs to be paint
  // invalidated.
  SetShouldDoFullPaintInvalidation();
}

}  // namespace blink
