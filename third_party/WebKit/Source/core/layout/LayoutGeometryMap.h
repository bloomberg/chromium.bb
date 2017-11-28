/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LayoutGeometryMap_h
#define LayoutGeometryMap_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/layout/LayoutGeometryMapStep.h"
#include "core/layout/MapCoordinatesFlags.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class PaintLayer;
class LayoutBoxModelObject;
class LayoutObject;
class TransformationMatrix;
class TransformState;

// Can be used while walking the layout tree to cache data about offsets and
// transforms.
class CORE_EXPORT LayoutGeometryMap {
  DISALLOW_NEW();

 public:
  LayoutGeometryMap(MapCoordinatesFlags = kUseTransforms);
  ~LayoutGeometryMap();

  MapCoordinatesFlags GetMapCoordinatesFlags() const {
    return map_coordinates_flags_;
  }

  FloatRect AbsoluteRect(const FloatRect& rect) const {
    return MapToAncestor(rect, nullptr).BoundingBox();
  }

  // Map to an ancestor. Will assert that the ancestor has been pushed onto this
  // map. A null ancestor maps through the LayoutView (including its scale
  // transform, if any). If the ancestor is the LayoutView, the scroll offset is
  // applied, but not the scale.
  FloatQuad MapToAncestor(const FloatRect&, const LayoutBoxModelObject*) const;

  // Called by code walking the layout or layer trees.
  void PushMappingsToAncestor(const PaintLayer*,
                              const PaintLayer* ancestor_layer);
  void PopMappingsToAncestor(const PaintLayer*);
  void PushMappingsToAncestor(
      const LayoutObject*,
      const LayoutBoxModelObject* ancestor_layout_object);

  // The following methods should only be called by layoutObjects inside a call
  // to pushMappingsToAncestor().

  // Push geometry info between this layoutObject and some ancestor. The
  // ancestor must be its container() or some stacking context between the
  // layoutObject and its container.
  void Push(const LayoutObject*,
            const LayoutSize&,
            GeometryInfoFlags = 0,
            LayoutSize offset_for_fixed_position = LayoutSize());
  void Push(const LayoutObject*,
            const TransformationMatrix&,
            GeometryInfoFlags = 0,
            LayoutSize offset_for_fixed_position = LayoutSize());

 private:
  void PopMappingsToAncestor(const LayoutBoxModelObject*);
  void MapToAncestor(TransformState&,
                     const LayoutBoxModelObject* ancestor = nullptr) const;

  void StepInserted(const LayoutGeometryMapStep&);
  void StepRemoved(const LayoutGeometryMapStep&);

  bool HasNonUniformStep() const { return non_uniform_steps_count_; }
  bool HasTransformStep() const { return transformed_steps_count_; }
  bool HasFixedPositionStep() const { return fixed_steps_count_; }

#ifndef NDEBUG
  void DumpSteps() const;
#endif

#if DCHECK_IS_ON()
  bool IsTopmostLayoutView(const LayoutObject*) const;
#endif

  typedef Vector<LayoutGeometryMapStep, 32> LayoutGeometryMapSteps;

  size_t insertion_position_;
  int non_uniform_steps_count_;
  int transformed_steps_count_;
  int fixed_steps_count_;
  LayoutGeometryMapSteps mapping_;
  LayoutSize accumulated_offset_;
  MapCoordinatesFlags map_coordinates_flags_;

  DISALLOW_COPY_AND_ASSIGN(LayoutGeometryMap);
};

}  // namespace blink

#endif  // LayoutGeometryMap_h
