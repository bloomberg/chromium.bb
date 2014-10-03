/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
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

#include "config.h"
#include "core/rendering/RenderSlider.h"

#include "core/InputTypeNames.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html/shadow/SliderThumbElement.h"
#include "core/rendering/RenderSliderThumb.h"
#include "wtf/MathExtras.h"

using std::min;

namespace blink {

const int RenderSlider::defaultTrackLength = 129;

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderFlexibleBox(element)
{
    // We assume RenderSlider works only with <input type=range>.
    ASSERT(element->type() == InputTypeNames::range);
}

RenderSlider::~RenderSlider()
{
}

int RenderSlider::baselinePosition(FontBaseline, bool /*firstLine*/, LineDirectionMode, LinePositionMode linePositionMode) const
{
    ASSERT(linePositionMode == PositionOnContainingLine);
    // FIXME: Patch this function for writing-mode.
    return height() + marginTop();
}

void RenderSlider::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    maxLogicalWidth = defaultTrackLength * style()->effectiveZoom();
    if (!style()->width().isPercent())
        minLogicalWidth = maxLogicalWidth;
}

inline SliderThumbElement* RenderSlider::sliderThumbElement() const
{
    return toSliderThumbElement(toElement(node())->userAgentShadowRoot()->getElementById(ShadowElementNames::sliderThumb()));
}

void RenderSlider::layout()
{
    // FIXME: Find a way to cascade appearance.
    // http://webkit.org/b/62535
    RenderBox* thumbBox = sliderThumbElement()->renderBox();
    if (thumbBox && thumbBox->isSliderThumb())
        static_cast<RenderSliderThumb*>(thumbBox)->updateAppearance(style());

    RenderFlexibleBox::layout();
}

bool RenderSlider::inDragMode() const
{
    return sliderThumbElement()->active();
}

} // namespace blink
