// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainterBase_h
#define BoxPainterBase_h

#include "core/style/ShadowData.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class Document;
class FloatRoundedRect;
class LayoutPoint;
class LayoutRect;
struct PaintInfo;

// Base class for box painting. Has no dependencies on the layout tree and thus
// provides functionality and definitions that can be shared between both legacy
// layout and LayoutNG. For performance reasons no virtual methods are utilized.
class BoxPainterBase {
  STACK_ALLOCATED();

 public:
  BoxPainterBase() {}

  static void PaintNormalBoxShadow(const PaintInfo&,
                                   const LayoutRect&,
                                   const ComputedStyle&,
                                   bool include_logical_left_edge = true,
                                   bool include_logical_right_edge = true);

  // The input rect should be the border rect. The outer bounds of the shadow
  // will be inset by border widths.
  static void PaintInsetBoxShadow(const PaintInfo&,
                                  const LayoutRect&,
                                  const ComputedStyle&,
                                  bool include_logical_left_edge = true,
                                  bool include_logical_right_edge = true);

  // This form is used by callers requiring special computation of the outer
  // bounds of the shadow. For example, TableCellPainter insets the bounds by
  // half widths of collapsed borders instead of the default whole widths.
  static void PaintInsetBoxShadowInBounds(
      const PaintInfo&,
      const FloatRoundedRect& bounds,
      const ComputedStyle&,
      bool include_logical_left_edge = true,
      bool include_logical_right_edge = true);

  static bool ShouldForceWhiteBackgroundForPrintEconomy(const Document&,
                                                        const ComputedStyle&);

  LayoutRect BoundsForDrawingRecorder(const PaintInfo&,
                                      const LayoutPoint& adjusted_paint_offset);
};

}  // namespace blink

#endif
