/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_model_object.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

namespace blink {

LayoutSVGModelObject::LayoutSVGModelObject(SVGElement* node)
    : LayoutObject(node) {}

bool LayoutSVGModelObject::IsChildAllowed(LayoutObject* child,
                                          const ComputedStyle&) const {
  return child->IsSVG() && !(child->IsSVGInline() || child->IsSVGInlineText());
}

void LayoutSVGModelObject::MapLocalToAncestor(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    MapCoordinatesFlags flags) const {
  SVGLayoutSupport::MapLocalToAncestor(this, ancestor, transform_state, flags);
}

LayoutRect LayoutSVGModelObject::AbsoluteVisualRect() const {
  return SVGLayoutSupport::VisualRectInAncestorSpace(*this, *View());
}

void LayoutSVGModelObject::MapAncestorToLocal(
    const LayoutBoxModelObject* ancestor,
    TransformState& transform_state,
    MapCoordinatesFlags flags) const {
  SVGLayoutSupport::MapAncestorToLocal(*this, ancestor, transform_state, flags);
}

const LayoutObject* LayoutSVGModelObject::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  return SVGLayoutSupport::PushMappingToContainer(this, ancestor_to_stop_at,
                                                  geometry_map);
}

void LayoutSVGModelObject::AbsoluteRects(
    Vector<IntRect>& rects,
    const LayoutPoint& accumulated_offset) const {
  IntRect rect = EnclosingIntRect(StrokeBoundingBox());
  rect.MoveBy(RoundedIntPoint(accumulated_offset));
  rects.push_back(rect);
}

void LayoutSVGModelObject::AbsoluteQuads(Vector<FloatQuad>& quads,
                                         MapCoordinatesFlags mode) const {
  quads.push_back(LocalToAbsoluteQuad(StrokeBoundingBox(), mode));
}

// This method is called from inside PaintOutline(), and since we call
// PaintOutline() while transformed to our coord system, return local coords.
void LayoutSVGModelObject::AddOutlineRects(
    Vector<LayoutRect>& rects,
    const LayoutPoint&,
    IncludeBlockVisualOverflowOrNot) const {
  rects.push_back(LayoutRect(VisualRectInLocalSVGCoordinates()));
}

FloatRect LayoutSVGModelObject::LocalBoundingBoxRectForAccessibility() const {
  return StrokeBoundingBox();
}

void LayoutSVGModelObject::WillBeDestroyed() {
  SVGResourcesCache::ClientDestroyed(*this);
  SVGResources::ClearClipPathFilterMask(*GetElement(), Style());
  LayoutObject::WillBeDestroyed();
}

void LayoutSVGModelObject::ComputeLayerHitTestRects(
    LayerHitTestRects& rects,
    TouchAction supported_fast_actions) const {
  // Using just the rect for the SVGRoot is good enough for now.
  SVGLayoutSupport::FindTreeRootObject(this)->ComputeLayerHitTestRects(
      rects, supported_fast_actions);
}

void LayoutSVGModelObject::AddLayerHitTestRects(
    LayerHitTestRects&,
    const PaintLayer* current_layer,
    const LayoutPoint& layer_offset,
    TouchAction supported_fast_actions,
    const LayoutRect& container_rect,
    TouchAction container_whitelisted_touch_action) const {
  // We don't walk into SVG trees at all - just report their container.
}

void LayoutSVGModelObject::StyleDidChange(StyleDifference diff,
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

    if (has_blend_mode_changed)
      SetNeedsPaintPropertyUpdate();
  }

  LayoutObject::StyleDidChange(diff, old_style);
  SVGResources::UpdateClipPathFilterMask(*GetElement(), old_style, StyleRef());
  SVGResourcesCache::ClientStyleChanged(*this, diff, StyleRef());
}

bool LayoutSVGModelObject::NodeAtPoint(HitTestResult&,
                                       const HitTestLocation&,
                                       const LayoutPoint&,
                                       HitTestAction) {
  NOTREACHED();
  return false;
}

}  // namespace blink
