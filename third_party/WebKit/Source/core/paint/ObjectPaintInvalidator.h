// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintInvalidator_h
#define ObjectPaintInvalidator_h

#include "core/CoreExport.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"

namespace blink {

class DisplayItemClient;
class LayoutBoxModelObject;
class LayoutObject;
class LayoutPoint;
class LayoutRect;
struct PaintInvalidatorContext;

class CORE_EXPORT ObjectPaintInvalidator {
  STACK_ALLOCATED();

 public:
  ObjectPaintInvalidator(const LayoutObject& object) : object_(object) {}

  static void ObjectWillBeDestroyed(const LayoutObject&);

  // This calls paintingLayer() which walks up the tree.
  // If possible, use the faster
  // PaintInvalidatorContext.paintingLayer.setNeedsRepaint().
  void SlowSetPaintingLayerNeedsRepaint();

  // TODO(wangxianzhu): Change the call sites to use the faster version if
  // possible.
  void SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
      const DisplayItemClient& client,
      PaintInvalidationReason reason) {
    SlowSetPaintingLayerNeedsRepaint();
    InvalidateDisplayItemClient(client, reason);
  }

  void InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
      PaintInvalidationReason);

  void InvalidatePaintOfPreviousVisualRect(
      const LayoutBoxModelObject& paint_invalidation_container,
      PaintInvalidationReason);

  // The caller should ensure the painting layer has been setNeedsRepaint before
  // calling this function.
  void InvalidateDisplayItemClient(const DisplayItemClient&,
                                   PaintInvalidationReason);

  // Actually do the paint invalidate of rect r for this object which has been
  // computed in the coordinate space of the GraphicsLayer backing of
  // |paintInvalidationContainer|. Note that this coordinate space is not the
  // same as the local coordinate space of |paintInvalidationContainer| in the
  // presence of layer squashing.
  void InvalidatePaintUsingContainer(
      const LayoutBoxModelObject& paint_invalidation_container,
      const LayoutRect&,
      PaintInvalidationReason);

  // Invalidate the paint of a specific subrectangle within a given object. The
  // rect is in the object's coordinate space.  If a DisplayItemClient is
  // specified, that client is invalidated rather than |m_object|.
  // Returns the visual rect that was invalidated (i.e, invalidation in the
  // space of the GraphicsLayer backing this LayoutObject).
  LayoutRect InvalidatePaintRectangle(const LayoutRect&, DisplayItemClient*);

  void InvalidatePaintIncludingNonCompositingDescendants();
  void InvalidatePaintIncludingNonSelfPaintingLayerDescendants(
      const LayoutBoxModelObject& paint_invalidation_container);

  LayoutPoint LocationInBacking() const;
  void SetLocationInBacking(const LayoutPoint&);

 private:
  void InvalidatePaintIncludingNonSelfPaintingLayerDescendantsInternal(
      const LayoutBoxModelObject& paint_invalidation_container);
  void SetBackingNeedsPaintInvalidationInRect(
      const LayoutBoxModelObject& paint_invalidation_container,
      const LayoutRect&,
      PaintInvalidationReason);

 protected:
  const LayoutObject& object_;
};

class ObjectPaintInvalidatorWithContext : public ObjectPaintInvalidator {
 public:
  ObjectPaintInvalidatorWithContext(const LayoutObject& object,
                                    const PaintInvalidatorContext& context)
      : ObjectPaintInvalidator(object), context_(context) {}

  PaintInvalidationReason InvalidatePaintIfNeeded() {
    return InvalidatePaintIfNeededWithComputedReason(
        ComputePaintInvalidationReason());
  }

  PaintInvalidationReason ComputePaintInvalidationReason();
  PaintInvalidationReason InvalidatePaintIfNeededWithComputedReason(
      PaintInvalidationReason);

  // This function generates a full invalidation, which means invalidating both
  // |oldVisualRect| and |newVisualRect|.  This is the default choice when
  // generating an invalidation, as it is always correct, albeit it may force
  // some extra painting.
  void FullyInvalidatePaint(PaintInvalidationReason,
                            const LayoutRect& old_visual_rect,
                            const LayoutRect& new_visual_rect);

  void InvalidatePaintRectangleWithContext(const LayoutRect&,
                                           PaintInvalidationReason);

 private:
  void InvalidateSelectionIfNeeded(PaintInvalidationReason);
  bool ParentFullyInvalidatedOnSameBacking();

  const PaintInvalidatorContext& context_;
};

// TODO(crbug.com/457415): We should not allow paint invalidation out of paint
// invalidation state.
class DisablePaintInvalidationStateAsserts {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DisablePaintInvalidationStateAsserts);

 public:
  DisablePaintInvalidationStateAsserts();

 private:
  AutoReset<bool> disabler_;
};

}  // namespace blink

#endif
