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

#ifndef LayoutSVGModelObject_h
#define LayoutSVGModelObject_h

#include "core/layout/LayoutObject.h"
#include "core/svg/SVGElement.h"

namespace blink {

// Most layoutObjects in the SVG layout tree will inherit from this class
// but not all. (e.g. LayoutSVGForeignObject, LayoutSVGBlock) thus methods
// required by SVG layoutObjects need to be declared on LayoutObject, but shared
// logic can go in this class or in SVGLayoutSupport.

class LayoutSVGModelObject : public LayoutObject {
 public:
  explicit LayoutSVGModelObject(SVGElement*);

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  LayoutRect AbsoluteVisualRect() const override;
  FloatRect VisualRectInLocalSVGCoordinates() const override {
    return local_visual_rect_;
  }

  void AbsoluteRects(Vector<IntRect>&,
                     const LayoutPoint& accumulated_offset) const final;
  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const override;
  FloatRect LocalBoundingBoxRectForAccessibility() const final;

  void MapLocalToAncestor(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags = kApplyContainerFlip) const final;
  void MapAncestorToLocal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags = kApplyContainerFlip) const final;
  const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const final;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void ComputeLayerHitTestRects(LayerHitTestRects&) const final;

  SVGElement* GetElement() const {
    return ToSVGElement(LayoutObject::GetNode());
  }

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVG || LayoutObject::IsOfType(type);
  }

 protected:
  void AddLayerHitTestRects(LayerHitTestRects&,
                            const PaintLayer* current_composited_layer,
                            const LayoutPoint& layer_offset,
                            const LayoutRect& container_rect) const final;
  void WillBeDestroyed() override;

 private:
  // LayoutSVGModelObject subclasses should use element() instead.
  void GetNode() const = delete;

  // This method should never be called, SVG uses a different nodeAtPoint method
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) final;
  IntRect AbsoluteElementBoundingBoxRect() const final;

 protected:
  FloatRect local_visual_rect_;
};

}  // namespace blink

#endif
