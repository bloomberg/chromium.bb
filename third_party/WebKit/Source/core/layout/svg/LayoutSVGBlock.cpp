/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/layout/svg/LayoutSVGBlock.h"

#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/style/ShadowList.h"
#include "core/svg/SVGElement.h"
#include "platform/geometry/TransformState.h"

namespace blink {

LayoutSVGBlock::LayoutSVGBlock(SVGElement* element)
    : LayoutBlockFlow(element) {}

bool LayoutSVGBlock::AllowsOverflowClip() const {
  // LayoutSVGBlock, used by Layout(SVGText|ForeignObject), is not allowed to
  // have overflow clip.
  // LayoutBlock assumes a layer to be present when the overflow clip
  // functionality is requested. Both Layout(SVGText|ForeignObject) return
  // 'NoPaintLayer' on 'layerTypeRequired'.
  // Fine for LayoutSVGText.
  //
  // If we want to support overflow rules for <foreignObject> we can choose
  // between two solutions:
  // a) make LayoutSVGForeignObject require layers and SVG layer aware
  // b) refactor overflow logic out of Layer (as suggested by dhyatt), which is
  //    a large task
  //
  // Until this is resolved, let this function return false, and create overflow
  // clip in PaintPropertyTreeBuilder and apply overflow clip in
  // SVGForeignObjectPainter.
  //
  // Note: This does NOT affect overflow handling on outer/inner <svg> elements
  // - this is handled manually by LayoutSVGRoot - which owns the documents
  // enclosing root layer and thus works fine.
  return false;
}

void LayoutSVGBlock::AbsoluteRects(Vector<IntRect>&, const LayoutPoint&) const {
  // This code path should never be taken for SVG, as we're assuming
  // useTransforms=true everywhere, absoluteQuads should be used.
  NOTREACHED();
}

void LayoutSVGBlock::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(this);
  LayoutBlockFlow::WillBeDestroyed();
}

void LayoutSVGBlock::UpdateFromStyle() {
  LayoutBlockFlow::UpdateFromStyle();
  SetFloating(false);
}

void LayoutSVGBlock::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  if (diff.NeedsFullLayout()) {
    SetNeedsBoundariesUpdate();
    if (diff.TransformChanged())
      SetNeedsTransformUpdate();
  }

  if (IsBlendingAllowed()) {
    bool has_blend_mode_changed =
        (old_style && old_style->HasBlendMode()) == !Style()->HasBlendMode();
    if (Parent() && has_blend_mode_changed)
      Parent()->DescendantIsolationRequirementsChanged(
          Style()->HasBlendMode() ? kDescendantIsolationRequired
                                  : kDescendantIsolationNeedsUpdate);
  }

  LayoutBlock::StyleDidChange(diff, old_style);
  SVGResourcesCache::ClientStyleChanged(this, diff, StyleRef());
}

void LayoutSVGBlock::MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                                        TransformState& transform_state,
                                        MapCoordinatesFlags flags) const {
  // Convert from local HTML coordinates to local SVG coordinates.
  transform_state.Move(LocationOffset());
  // Apply other mappings on local SVG coordinates.
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

void LayoutSVGBlock::MapAncestorToLocal(const LayoutBoxModelObject* ancestor,
                                        TransformState& transform_state,
                                        MapCoordinatesFlags flags) const {
  if (this == ancestor)
    return;

  // Map to local SVG coordinates.
  SVGLayoutSupport::MapAncestorToLocal(*this, ancestor, transform_state, flags);
  // Convert from local SVG coordinates to local HTML coordinates.
  transform_state.Move(LocationOffset());
}

const LayoutObject* LayoutSVGBlock::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  // Convert from local HTML coordinates to local SVG coordinates.
  geometry_map.Push(this, LocationOffset());
  // Apply other mappings on local SVG coordinates.
  return SVGLayoutSupport::PushMappingToContainer(this, ancestor_to_stop_at,
                                                  geometry_map);
}

LayoutRect LayoutSVGBlock::AbsoluteVisualRect() const {
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View());
}

bool LayoutSVGBlock::MapToVisualRectInAncestorSpaceInternal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    VisualRectFlags) const {
  transform_state.Flatten();
  LayoutRect rect(transform_state.LastPlanarQuad().BoundingBox());
  // Convert from local HTML coordinates to local SVG coordinates.
  rect.MoveBy(Location());
  // Apply other mappings on local SVG coordinates.
  bool retval = SVGLayoutSupport::MapToVisualRectInAncestorSpace(
      *this, ancestor, FloatRect(rect), rect);
  transform_state.SetQuad(FloatQuad(FloatRect(rect)));
  return retval;
}

bool LayoutSVGBlock::NodeAtPoint(HitTestResult&,
                                 const HitTestLocation&,
                                 const LayoutPoint&,
                                 HitTestAction) {
  NOTREACHED();
  return false;
}

}  // namespace blink
