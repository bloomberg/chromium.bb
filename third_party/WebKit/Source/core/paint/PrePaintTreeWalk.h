// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrePaintTreeWalk_h
#define PrePaintTreeWalk_h

#include "core/paint/ClipRect.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

class FrameView;
class GeometryMapper;
class LayoutObject;
struct PrePaintTreeWalkContext;

// This class walks the whole layout tree, beginning from the root FrameView,
// across frame boundaries. Helper classes are called for each tree node to
// perform actual actions.  It expects to be invoked in InPrePaint phase.
class PrePaintTreeWalk {
 public:
  PrePaintTreeWalk(GeometryMapper& geometryMapper)
      : m_paintInvalidator(geometryMapper), m_geometryMapper(geometryMapper) {}
  void walk(FrameView& rootFrame);

 private:
  void walk(FrameView&, const PrePaintTreeWalkContext&);
  void walk(const LayoutObject&, const PrePaintTreeWalkContext&);

  // Invalidates paint-layer painting optimizations, such as subsequence caching
  // and empty paint phase optimizations if clips from the context have changed.
  void invalidatePaintLayerOptimizationsIfNeeded(const LayoutObject&,
                                                 PrePaintTreeWalkContext&);

  // Returns the clip applied to children for the given
  // contaiing block context + effect, in the space of ancestorState adjusted
  // by ancestorPaintOffset. Sets hasClip to true if a clip was applied.
  FloatClipRect clipRectForContext(
      const PaintPropertyTreeBuilderContext::ContainingBlockContext&,
      const EffectPaintPropertyNode*,
      const PropertyTreeState& ancestorState,
      const LayoutPoint& ancestorPaintOffset,
      bool& hasClip);

  PaintPropertyTreeBuilder m_propertyTreeBuilder;
  PaintInvalidator m_paintInvalidator;
  GeometryMapper& m_geometryMapper;
};

}  // namespace blink

#endif  // PrePaintTreeWalk_h
