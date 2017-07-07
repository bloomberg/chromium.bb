// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPainter_h
#define ObjectPainter_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class Color;
class GraphicsContext;
class LayoutPoint;
struct PaintInfo;
class LayoutObject;

class ObjectPainter {
  STACK_ALLOCATED();

 public:
  ObjectPainter(const LayoutObject& layout_object)
      : layout_object_(layout_object) {}

  void PaintOutline(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintInlineChildrenOutlines(const PaintInfo&,
                                   const LayoutPoint& paint_offset);
  void AddPDFURLRectIfNeeded(const PaintInfo&, const LayoutPoint& paint_offset);

  static void DrawLineForBoxSide(GraphicsContext&,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2,
                                 BoxSide,
                                 Color,
                                 EBorderStyle,
                                 int adjbw1,
                                 int adjbw2,
                                 bool antialias = false);

  // Paints the object atomically as if it created a new stacking context, for:
  // - inline blocks, inline tables, inline-level replaced elements (Section
  //   7.2.1.4 in http://www.w3.org/TR/CSS2/zindex.html#painting-order),
  // - non-positioned floating objects (Section 5 in
  //   http://www.w3.org/TR/CSS2/zindex.html#painting-order),
  // - flex items (http://www.w3.org/TR/css-flexbox-1/#painting),
  // - grid items (http://www.w3.org/TR/css-grid-1/#z-order),
  // - custom scrollbar parts.
  // Also see core/paint/README.md.
  //
  // It is expected that the caller will call this function independent of the
  // value of paintInfo.phase, and this function will do atomic paint (for
  // PaintPhaseForeground), normal paint (for PaintPhaseSelection and
  // PaintPhaseTextClip) or nothing (other paint phases) according to
  // paintInfo.phase.
  void PaintAllPhasesAtomically(const PaintInfo&,
                                const LayoutPoint& paint_offset);

  // We compute paint offsets during the pre-paint tree walk (PrePaintTreeWalk).
  // This check verifies that the paint offset computed during pre-paint matches
  // the actual paint offset during paint.
  void CheckPaintOffset(const PaintInfo& paint_info,
                        const LayoutPoint& paint_offset) {
#if DCHECK_IS_ON()
    // For now this works for SPv2 only because of complexities of paint
    // invalidation containers in SPv1.
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      DoCheckPaintOffset(paint_info, paint_offset);
#endif
  }

 private:
  static void DrawDashedOrDottedBoxSide(GraphicsContext&,
                                        int x1,
                                        int y1,
                                        int x2,
                                        int y2,
                                        BoxSide,
                                        Color,
                                        int thickness,
                                        EBorderStyle,
                                        bool antialias);
  static void DrawDoubleBoxSide(GraphicsContext&,
                                int x1,
                                int y1,
                                int x2,
                                int y2,
                                int length,
                                BoxSide,
                                Color,
                                float thickness,
                                int adjacent_width1,
                                int adjacent_width2,
                                bool antialias);
  static void DrawRidgeOrGrooveBoxSide(GraphicsContext&,
                                       int x1,
                                       int y1,
                                       int x2,
                                       int y2,
                                       BoxSide,
                                       Color,
                                       EBorderStyle,
                                       int adjacent_width1,
                                       int adjacent_width2,
                                       bool antialias);
  static void DrawSolidBoxSide(GraphicsContext&,
                               int x1,
                               int y1,
                               int x2,
                               int y2,
                               BoxSide,
                               Color,
                               int adjacent_width1,
                               int adjacent_width2,
                               bool antialias);

#if DCHECK_IS_ON()
  void DoCheckPaintOffset(const PaintInfo&, const LayoutPoint& paint_offset);
#endif

  const LayoutObject& layout_object_;
};

}  // namespace blink

#endif
