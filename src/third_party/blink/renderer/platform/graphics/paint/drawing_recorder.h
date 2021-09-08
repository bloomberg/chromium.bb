// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_RECORDER_H_

#include "base/auto_reset.h"
#include "base/dcheck_is_on.h"
#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT DrawingRecorder {
  STACK_ALLOCATED();

 public:
  static bool UseCachedDrawingIfPossible(GraphicsContext& context,
                                         const DisplayItemClient& client,
                                         DisplayItem::Type type) {
    return context.GetPaintController().UseCachedItemIfPossible(client, type);
  }

  static bool UseCachedDrawingIfPossible(GraphicsContext& context,
                                         const DisplayItemClient& client,
                                         PaintPhase phase) {
    return UseCachedDrawingIfPossible(
        context, client, DisplayItem::PaintPhaseToDrawingType(phase));
  }

  // See DisplayItem::VisualRect() for the definition of visual rect.
  DrawingRecorder(GraphicsContext&,
                  const DisplayItemClient&,
                  DisplayItem::Type,
                  const IntRect& visual_rect);

  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  PaintPhase phase,
                  const IntRect& visual_rect)
      : DrawingRecorder(context,
                        client,
                        DisplayItem::PaintPhaseToDrawingType(phase),
                        visual_rect) {}

  // This form is for recording with a transient paint controller, e.g. when
  // we are recording into a PaintRecordBuilder, where visual rect doesn't
  // matter.
  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  DisplayItem::Type type)
      : DrawingRecorder(context, client, type, IntRect()) {
#if DCHECK_IS_ON()
    DCHECK_EQ(PaintController::kTransient,
              context.GetPaintController().GetUsage());
#endif
  }
  DrawingRecorder(GraphicsContext& context,
                  const DisplayItemClient& client,
                  PaintPhase phase)
      : DrawingRecorder(context,
                        client,
                        DisplayItem::PaintPhaseToDrawingType(phase)) {}

  ~DrawingRecorder();

  // Sometimes we don't the the exact visual rect when we create a
  // DrawingRecorder. This method allows visual rect to be added during
  // painting.
  void UniteVisualRect(const IntRect& rect) { visual_rect_.Unite(rect); }

 private:
  GraphicsContext& context_;
  const DisplayItemClient& client_;
  const DisplayItem::Type type_;
  IntRect visual_rect_;
  absl::optional<DOMNodeId> dom_node_id_to_restore_;

  DISALLOW_COPY_AND_ASSIGN(DrawingRecorder);
};

#if DCHECK_IS_ON()
class DisableListModificationCheck {
  STACK_ALLOCATED();

 public:
  DisableListModificationCheck();

 private:
  base::AutoReset<bool> disabler_;

  DISALLOW_COPY_AND_ASSIGN(DisableListModificationCheck);
};
#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_RECORDER_H_
