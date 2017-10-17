// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DrawingRecorder_h
#define DrawingRecorder_h

#include "platform/PlatformExport.h"

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/AutoReset.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT DrawingRecorder final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  WTF_MAKE_NONCOPYABLE(DrawingRecorder);

 public:
  static bool UseCachedDrawingIfPossible(GraphicsContext& context,
                                         const DisplayItemClient& client,
                                         DisplayItem::Type type) {
    return context.GetPaintController().UseCachedDrawingIfPossible(client,
                                                                   type);
  }

  static bool UseCachedDrawingIfPossible(GraphicsContext& context,
                                         const DisplayItemClient& client,
                                         PaintPhase phase) {
    return UseCachedDrawingIfPossible(
        context, client, DisplayItem::PaintPhaseToDrawingType(phase));
  }

  DrawingRecorder(GraphicsContext&,
                  const DisplayItemClient&,
                  DisplayItem::Type,
                  const IntRect& bounds);

  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  PaintPhase phase,
                  const IntRect& bounds)
      : DrawingRecorder(context,
                        client,
                        DisplayItem::PaintPhaseToDrawingType(phase),
                        bounds) {}

  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  DisplayItem::Type type,
                  const FloatRect& bounds)
      // Use the enclosing int rect, since pixel-snapping may be applied to the
      // bounds of the object during painting. Potentially expanding the bounds
      // by a pixel or two also does not affect correctness, and is very
      // unlikely to matter for performance.
      : DrawingRecorder(context, client, type, EnclosingIntRect(bounds)) {}

  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  PaintPhase phase,
                  const FloatRect& bounds)
      : DrawingRecorder(context,
                        client,
                        DisplayItem::PaintPhaseToDrawingType(phase),
                        bounds) {}

  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  DisplayItem::Type type,
                  const LayoutRect& bounds)
      : DrawingRecorder(context, client, type, EnclosingIntRect(bounds)) {}

  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  PaintPhase phase,
                  const LayoutRect& bounds)
      : DrawingRecorder(context,
                        client,
                        DisplayItem::PaintPhaseToDrawingType(phase),
                        bounds) {}

  ~DrawingRecorder();

  void SetKnownToBeOpaque() {
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
    known_to_be_opaque_ = true;
  }

 private:
  GraphicsContext& context_;
  const DisplayItemClient& client_;
  const DisplayItem::Type type_;

  // True if there are no transparent areas. Only used for SlimmingPaintV2.
  bool known_to_be_opaque_;
  // The bounds of the area being recorded.
  IntRect bounds_;

#if DCHECK_IS_ON()
  // Ensures the list size does not change during the recorder's scope.
  size_t initial_display_item_list_size_;
#endif
};

#if DCHECK_IS_ON()
class DisableListModificationCheck {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(DisableListModificationCheck);

 public:
  DisableListModificationCheck();

 private:
  AutoReset<bool> disabler_;
};
#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // DrawingRecorder_h
