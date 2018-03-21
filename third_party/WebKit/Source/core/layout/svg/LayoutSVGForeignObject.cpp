/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/layout/svg/LayoutSVGForeignObject.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGForeignObjectPainter.h"
#include "core/svg/SVGForeignObjectElement.h"

namespace blink {

LayoutSVGForeignObject::LayoutSVGForeignObject(SVGForeignObjectElement* node)
    : LayoutSVGBlock(node), needs_transform_update_(true) {}

LayoutSVGForeignObject::~LayoutSVGForeignObject() = default;

bool LayoutSVGForeignObject::IsChildAllowed(LayoutObject* child,
                                            const ComputedStyle& style) const {
  // Disallow arbitary SVG content. Only allow proper <svg xmlns="svgNS">
  // subdocuments.
  return !child->IsSVGChild();
}

void LayoutSVGForeignObject::Paint(const PaintInfo& paint_info,
                                   const LayoutPoint&) const {
  SVGForeignObjectPainter(*this).Paint(paint_info);
}

LayoutUnit LayoutSVGForeignObject::ElementX() const {
  return LayoutUnit(
      roundf(SVGLengthContext(ToSVGElement(GetNode()))
                 .ValueForLength(StyleRef().SvgStyle().X(), StyleRef(),
                                 SVGLengthMode::kWidth)));
}

LayoutUnit LayoutSVGForeignObject::ElementY() const {
  return LayoutUnit(
      roundf(SVGLengthContext(ToSVGElement(GetNode()))
                 .ValueForLength(StyleRef().SvgStyle().Y(), StyleRef(),
                                 SVGLengthMode::kHeight)));
}

LayoutUnit LayoutSVGForeignObject::ElementWidth() const {
  return LayoutUnit(SVGLengthContext(ToSVGElement(GetNode()))
                        .ValueForLength(StyleRef().Width(), StyleRef(),
                                        SVGLengthMode::kWidth));
}

LayoutUnit LayoutSVGForeignObject::ElementHeight() const {
  return LayoutUnit(SVGLengthContext(ToSVGElement(GetNode()))
                        .ValueForLength(StyleRef().Height(), StyleRef(),
                                        SVGLengthMode::kHeight));
}

void LayoutSVGForeignObject::UpdateLogicalWidth() {
  SetLogicalWidth(StyleRef().IsHorizontalWritingMode() ? ElementWidth()
                                                       : ElementHeight());
}

void LayoutSVGForeignObject::ComputeLogicalHeight(
    LayoutUnit,
    LayoutUnit logical_top,
    LogicalExtentComputedValues& computed_values) const {
  computed_values.extent_ =
      StyleRef().IsHorizontalWritingMode() ? ElementHeight() : ElementWidth();
  computed_values.position_ = logical_top;
}

void LayoutSVGForeignObject::UpdateLayout() {
  DCHECK(NeedsLayout());

  SVGForeignObjectElement* foreign = ToSVGForeignObjectElement(GetNode());

  bool update_cached_boundaries_in_parents = false;
  if (needs_transform_update_) {
    local_transform_ =
        foreign->CalculateTransform(SVGElement::kIncludeMotionTransform);
    needs_transform_update_ = false;
    update_cached_boundaries_in_parents = true;
  }

  LayoutRect old_viewport = FrameRect();

  // Set box origin to the foreignObject x/y translation, so positioned objects
  // in XHTML content get correct positions. A regular LayoutBoxModelObject
  // would pull this information from ComputedStyle - in SVG those properties
  // are ignored for non <svg> elements, so we mimic what happens when
  // specifying them through CSS.
  SetX(ElementX());
  SetY(ElementY());

  bool layout_changed = EverHadLayout() && SelfNeedsLayout();
  LayoutBlock::UpdateLayout();
  DCHECK(!NeedsLayout());

  // If our bounds changed, notify the parents.
  if (!update_cached_boundaries_in_parents)
    update_cached_boundaries_in_parents = old_viewport != FrameRect();
  if (update_cached_boundaries_in_parents)
    LayoutSVGBlock::SetNeedsBoundariesUpdate();

  // Invalidate all resources of this client if our layout changed.
  if (layout_changed)
    SVGResourcesCache::ClientLayoutChanged(*this);
}

bool LayoutSVGForeignObject::NodeAtFloatPoint(HitTestResult& result,
                                              const FloatPoint& point_in_parent,
                                              HitTestAction hit_test_action) {
  // Embedded content is drawn in the foreground phase.
  if (hit_test_action != kHitTestForeground)
    return false;

  AffineTransform local_transform = LocalSVGTransform();
  if (!local_transform.IsInvertible())
    return false;

  FloatPoint local_point = local_transform.Inverse().MapPoint(point_in_parent);

  // Early exit if local point is not contained in clipped viewport area
  if (SVGLayoutSupport::IsOverflowHidden(*this) &&
      !FrameRect().Contains(LayoutPoint(local_point)))
    return false;

  // FOs establish a stacking context, so we need to hit-test all layers.
  HitTestLocation hit_test_location(local_point);
  return LayoutBlock::NodeAtPoint(result, hit_test_location, LayoutPoint(),
                                  kHitTestForeground) ||
         LayoutBlock::NodeAtPoint(result, hit_test_location, LayoutPoint(),
                                  kHitTestFloat) ||
         LayoutBlock::NodeAtPoint(result, hit_test_location, LayoutPoint(),
                                  kHitTestChildBlockBackgrounds);
}

bool LayoutSVGForeignObject::NodeAtPoint(
    HitTestResult& result,
    const HitTestLocation& location_in_parent,
    const LayoutPoint& accumulated_offset,
    HitTestAction hit_test_action) {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
    return NodeAtFloatPoint(result, FloatPoint(accumulated_offset),
                            hit_test_action);
  }
  NOTREACHED();
  return false;
}

PaintLayerType LayoutSVGForeignObject::LayerTypeRequired() const {
  if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return LayoutBlockFlow::LayerTypeRequired();
  return kNoPaintLayer;
}

bool LayoutSVGForeignObject::AllowsOverflowClip() const {
  return RuntimeEnabledFeatures::SlimmingPaintV175Enabled();
}

void LayoutSVGForeignObject::StyleDidChange(StyleDifference diff,
                                            const ComputedStyle* old_style) {
  LayoutSVGBlock::StyleDidChange(diff, old_style);

  if (old_style && (SVGLayoutSupport::IsOverflowHidden(*old_style) !=
                    SVGLayoutSupport::IsOverflowHidden(StyleRef()))) {
    // See NeedsOverflowClip() in PaintPropertyTreeBuilder for the reason.
    SetNeedsPaintPropertyUpdate();
  }
}

}  // namespace blink
