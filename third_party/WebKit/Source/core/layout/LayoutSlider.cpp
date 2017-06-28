/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Publicw
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/layout/LayoutSlider.h"

#include "core/InputTypeNames.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/SliderThumbElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutSliderThumb.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

const int LayoutSlider::kDefaultTrackLength = 129;

LayoutSlider::LayoutSlider(HTMLInputElement* element)
    : LayoutFlexibleBox(element) {
  // We assume LayoutSlider works only with <input type=range>.
  DCHECK_EQ(element->type(), InputTypeNames::range);
}

LayoutSlider::~LayoutSlider() {}

int LayoutSlider::BaselinePosition(FontBaseline,
                                   bool /*firstLine*/,
                                   LineDirectionMode,
                                   LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  // FIXME: Patch this function for writing-mode.
  return (Size().Height() + MarginTop()).ToInt();
}

void LayoutSlider::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  max_logical_width =
      LayoutUnit(kDefaultTrackLength * Style()->EffectiveZoom());
  if (!Style()->Width().IsPercentOrCalc())
    min_logical_width = max_logical_width;
}

inline SliderThumbElement* LayoutSlider::GetSliderThumbElement() const {
  return ToSliderThumbElement(
      ToElement(GetNode())->UserAgentShadowRoot()->getElementById(
          ShadowElementNames::SliderThumb()));
}

void LayoutSlider::UpdateLayout() {
  // FIXME: Find a way to cascade appearance.
  // http://webkit.org/b/62535
  LayoutBox* thumb_box = GetSliderThumbElement()->GetLayoutBox();
  if (thumb_box && thumb_box->IsSliderThumb())
    ToLayoutSliderThumb(thumb_box)->UpdateAppearance(StyleRef());

  LayoutFlexibleBox::UpdateLayout();
}

bool LayoutSlider::InDragMode() const {
  return GetSliderThumbElement()->IsActive();
}

}  // namespace blink
