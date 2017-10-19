/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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
 */

#include "core/layout/svg/LayoutSVGGradientStop.h"

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGGradientElement.h"
#include "core/svg/SVGStopElement.h"

namespace blink {

LayoutSVGGradientStop::LayoutSVGGradientStop(SVGStopElement* element)
    : LayoutObject(element) {}

LayoutSVGGradientStop::~LayoutSVGGradientStop() {}

void LayoutSVGGradientStop::StyleDidChange(StyleDifference diff,
                                           const ComputedStyle* old_style) {
  LayoutObject::StyleDidChange(diff, old_style);
  if (!diff.HasDifference())
    return;

  // <stop> elements should only be allowed to make layoutObjects under gradient
  // elements but I can imagine a few cases we might not be catching, so let's
  // not crash if our parent isn't a gradient.
  SVGGradientElement* gradient = GradientElement();
  if (!gradient)
    return;

  LayoutObject* layout_object = gradient->GetLayoutObject();
  if (!layout_object)
    return;

  LayoutSVGResourceContainer* container =
      ToLayoutSVGResourceContainer(layout_object);
  container->RemoveAllClientsFromCache();
}

void LayoutSVGGradientStop::UpdateLayout() {
  ClearNeedsLayout();
}

SVGGradientElement* LayoutSVGGradientStop::GradientElement() const {
  ContainerNode* parent_node = GetNode()->parentNode();
  DCHECK(parent_node);
  return IsSVGGradientElement(*parent_node) ? ToSVGGradientElement(parent_node)
                                            : nullptr;
}

}  // namespace blink
