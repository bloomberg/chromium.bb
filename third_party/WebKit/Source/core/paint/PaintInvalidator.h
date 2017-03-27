// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidator_h
#define PaintInvalidator_h

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/GeometryMapper.h"
#include "wtf/Vector.h"

namespace blink {

class FrameView;
class LayoutBoxModelObject;
class LayoutObject;
class PaintLayer;
struct PaintPropertyTreeBuilderContext;

struct PaintInvalidatorContext {
  PaintInvalidatorContext(
      const PaintPropertyTreeBuilderContext& treeBuilderContext,
      GeometryMapper& geometryMapper)
      : parentContext(nullptr),
        m_treeBuilderContext(treeBuilderContext),
        m_geometryMapper(geometryMapper) {}

  PaintInvalidatorContext(
      const PaintPropertyTreeBuilderContext& treeBuilderContext,
      const PaintInvalidatorContext& parentContext)
      : parentContext(&parentContext),
        forcedSubtreeInvalidationFlags(
            parentContext.forcedSubtreeInvalidationFlags),
        paintInvalidationContainer(parentContext.paintInvalidationContainer),
        paintInvalidationContainerForStackedContents(
            parentContext.paintInvalidationContainerForStackedContents),
        paintingLayer(parentContext.paintingLayer),
        m_treeBuilderContext(treeBuilderContext),
        m_geometryMapper(parentContext.m_geometryMapper) {}

  // This method is virtual temporarily to adapt PaintInvalidatorContext and the
  // legacy PaintInvalidationState for code shared by old code and new code.
  virtual void mapLocalRectToVisualRectInBacking(const LayoutObject&,
                                                 LayoutRect&) const;

  const PaintInvalidatorContext* parentContext;

  enum ForcedSubtreeInvalidationFlag {
    ForcedSubtreeInvalidationChecking = 1 << 0,
    ForcedSubtreeInvalidationRectUpdate = 1 << 1,
    ForcedSubtreeFullInvalidation = 1 << 2,
    ForcedSubtreeFullInvalidationForStackedContents = 1 << 3,
    ForcedSubtreeSVGResourceChange = 1 << 4,
    // TODO(crbug.com/637313): This is temporary before we support filters in
    // paint property tree.
    ForcedSubtreeSlowPathRect = 1 << 5,

    // The paint invalidation tree walk invalidates paint caches, such as
    // DisplayItemClients and subsequence caches, and also the regions
    // into which objects raster pixels. When this flag is set, raster region
    // invalidations are not issued.
    //
    // Context: some objects in this paint walk, for example SVG resource
    // container subtress, don't actually have any raster regions, because they
    // are used as "painting subroutines" for one or more other locations in
    // SVG.
    ForcedSubtreeNoRasterInvalidation = 1 << 6,
  };
  unsigned forcedSubtreeInvalidationFlags = 0;

  // The following fields can be null only before
  // PaintInvalidator::updateContext().

  // The current paint invalidation container for normal flow objects.
  // It is the enclosing composited object.
  const LayoutBoxModelObject* paintInvalidationContainer = nullptr;

  // The current paint invalidation container for stacked contents (stacking
  // contexts or positioned objects).  It is the nearest ancestor composited
  // object which establishes a stacking context.  See
  // Source/core/paint/README.md ### PaintInvalidationState for details on how
  // stacked contents' paint invalidation containers differ.
  const LayoutBoxModelObject* paintInvalidationContainerForStackedContents =
      nullptr;

  PaintLayer* paintingLayer = nullptr;

  // Store the old visual rect in the paint invalidation backing's coordinates.
  // It does *not* account for composited scrolling.
  // See LayoutObject::adjustVisualRectForCompositedScrolling().
  LayoutRect oldVisualRect;
  // Use LayoutObject::visualRect() to get the new visual rect.

  // Store the origin of the object's local coordinates in the paint
  // invalidation backing's coordinates. They are used to detect layoutObject
  // shifts that force a full invalidation and invalidation check in subtree.
  // The points do *not* account for composited scrolling. See
  // LayoutObject::adjustVisualRectForCompositedScrolling().
  LayoutPoint oldLocation;
  LayoutPoint newLocation;

 private:
  friend class PaintInvalidator;
  const PaintPropertyTreeBuilderContext& m_treeBuilderContext;
  GeometryMapper& m_geometryMapper;
};

class PaintInvalidator {
 public:
  void invalidatePaintIfNeeded(FrameView&, PaintInvalidatorContext&);
  void invalidatePaintIfNeeded(const LayoutObject&, PaintInvalidatorContext&);

  // Process objects needing paint invalidation on the next frame.
  // See the definition of PaintInvalidationDelayedFull for more details.
  void processPendingDelayedPaintInvalidations();

 private:
  friend struct PaintInvalidatorContext;
  template <typename Rect, typename Point>
  static LayoutRect mapLocalRectToVisualRectInBacking(
      const LayoutObject&,
      const Rect&,
      const PaintInvalidatorContext&);

  ALWAYS_INLINE LayoutRect
  computeVisualRectInBacking(const LayoutObject&,
                             const PaintInvalidatorContext&);
  ALWAYS_INLINE LayoutPoint
  computeLocationInBacking(const LayoutObject&, const PaintInvalidatorContext&);
  ALWAYS_INLINE void updatePaintingLayer(const LayoutObject&,
                                         PaintInvalidatorContext&);
  ALWAYS_INLINE void updatePaintInvalidationContainer(const LayoutObject&,
                                                      PaintInvalidatorContext&);
  ALWAYS_INLINE void updateVisualRect(const LayoutObject&,
                                      PaintInvalidatorContext&);

  Vector<const LayoutObject*> m_pendingDelayedPaintInvalidations;
};

}  // namespace blink

#endif  // PaintInvalidator_h
