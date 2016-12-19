/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
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

#include "core/layout/svg/LayoutSVGViewportContainer.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/svg/SVGSVGElement.h"

namespace blink {

LayoutSVGViewportContainer::LayoutSVGViewportContainer(SVGSVGElement* node)
    : LayoutSVGContainer(node),
      m_isLayoutSizeChanged(false),
      m_needsTransformUpdate(true) {}

void LayoutSVGViewportContainer::layout() {
  DCHECK(needsLayout());
  DCHECK(isSVGSVGElement(element()));

  const SVGSVGElement* svg = toSVGSVGElement(element());
  m_isLayoutSizeChanged = selfNeedsLayout() && svg->hasRelativeLengths();

  if (selfNeedsLayout()) {
    SVGLengthContext lengthContext(svg);
    FloatRect oldViewport = m_viewport;
    m_viewport = FloatRect(svg->x()->currentValue()->value(lengthContext),
                           svg->y()->currentValue()->value(lengthContext),
                           svg->width()->currentValue()->value(lengthContext),
                           svg->height()->currentValue()->value(lengthContext));
    if (oldViewport != m_viewport) {
      setNeedsBoundariesUpdate();
      // The transform depends on viewport values.
      setNeedsTransformUpdate();
    }
  }

  LayoutSVGContainer::layout();
}

void LayoutSVGViewportContainer::setNeedsTransformUpdate() {
  setMayNeedPaintInvalidationSubtree();
  if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
    // The transform paint property relies on the SVG transform being up-to-date
    // (see: PaintPropertyTreeBuilder::updateTransformForNonRootSVG).
    setNeedsPaintPropertyUpdate();
  }
  m_needsTransformUpdate = true;
}

SVGTransformChange LayoutSVGViewportContainer::calculateLocalTransform() {
  if (!m_needsTransformUpdate)
    return SVGTransformChange::None;

  DCHECK(isSVGSVGElement(element()));
  const SVGSVGElement* svg = toSVGSVGElement(element());
  SVGTransformChangeDetector changeDetector(m_localToParentTransform);
  m_localToParentTransform =
      AffineTransform::translation(m_viewport.x(), m_viewport.y()) *
      svg->viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
  m_needsTransformUpdate = false;
  return changeDetector.computeChange(m_localToParentTransform);
}

bool LayoutSVGViewportContainer::nodeAtFloatPoint(
    HitTestResult& result,
    const FloatPoint& pointInParent,
    HitTestAction action) {
  // Respect the viewport clip which is in parent coordinates.
  if (SVGLayoutSupport::isOverflowHidden(this)) {
    if (!m_viewport.contains(pointInParent))
      return false;
  }
  return LayoutSVGContainer::nodeAtFloatPoint(result, pointInParent, action);
}

}  // namespace blink
