// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutObjectDrawingRecorder_h
#define LayoutObjectDrawingRecorder_h

#include "core/layout/LayoutObject.h"
#include "core/paint/PaintPhase.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"

namespace blink {

class GraphicsContext;

// Convenience wrapper of DrawingRecorder for LayoutObject painters.
class LayoutObjectDrawingRecorder final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  static bool UseCachedDrawingIfPossible(GraphicsContext& context,
                                         const LayoutObject& layout_object,
                                         DisplayItem::Type display_item_type) {
    return DrawingRecorder::UseCachedDrawingIfPossible(context, layout_object,
                                                       display_item_type);
  }

  static bool UseCachedDrawingIfPossible(GraphicsContext& context,
                                         const LayoutObject& layout_object,
                                         PaintPhase phase) {
    return UseCachedDrawingIfPossible(
        context, layout_object, DisplayItem::PaintPhaseToDrawingType(phase));
  }

  LayoutObjectDrawingRecorder(GraphicsContext& context,
                              const LayoutObject& layout_object,
                              DisplayItem::Type display_item_type,
                              const FloatRect& clip) {
    drawing_recorder_.emplace(context, layout_object, display_item_type, clip);
  }

  LayoutObjectDrawingRecorder(GraphicsContext& context,
                              const LayoutObject& layout_object,
                              DisplayItem::Type display_item_type,
                              const LayoutRect& clip)
      : LayoutObjectDrawingRecorder(context,
                                    layout_object,
                                    display_item_type,
                                    FloatRect(clip)) {}

  LayoutObjectDrawingRecorder(GraphicsContext& context,
                              const LayoutObject& layout_object,
                              PaintPhase phase,
                              const FloatRect& clip)
      : LayoutObjectDrawingRecorder(context,
                                    layout_object,
                                    DisplayItem::PaintPhaseToDrawingType(phase),
                                    clip) {}

  LayoutObjectDrawingRecorder(GraphicsContext& context,
                              const LayoutObject& layout_object,
                              PaintPhase phase,
                              const LayoutRect& clip)
      : LayoutObjectDrawingRecorder(context,
                                    layout_object,
                                    DisplayItem::PaintPhaseToDrawingType(phase),
                                    FloatRect(clip)) {}

  LayoutObjectDrawingRecorder(GraphicsContext& context,
                              const LayoutObject& layout_object,
                              PaintPhase phase,
                              const IntRect& clip)
      : LayoutObjectDrawingRecorder(context,
                                    layout_object,
                                    DisplayItem::PaintPhaseToDrawingType(phase),
                                    FloatRect(clip)) {}

  void SetKnownToBeOpaque() {
    DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
    drawing_recorder_->SetKnownToBeOpaque();
  }

 private:
  Optional<DrawingRecorder> drawing_recorder_;
};

}  // namespace blink

#endif  // LayoutObjectDrawingRecorder_h
