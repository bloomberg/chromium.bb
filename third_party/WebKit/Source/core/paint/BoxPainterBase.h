// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainterBase_h
#define BoxPainterBase_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/style/ShadowData.h"
#include "core/style/StyleImage.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Allocator.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink {

class ComputedStyle;
class Document;
class FloatRoundedRect;
class LayoutPoint;
class LayoutRect;
class FillLayer;
class LayoutRectOutsets;
class ImageResourceObserver;
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

  static void PaintBorder(const ImageResourceObserver&,
                          const Document&,
                          Node*,
                          const PaintInfo&,
                          const LayoutRect&,
                          const ComputedStyle&,
                          BackgroundBleedAvoidance = kBackgroundBleedNone,
                          bool include_logical_left_edge = true,
                          bool include_logical_right_edge = true);

  static bool ShouldForceWhiteBackgroundForPrintEconomy(const Document&,
                                                        const ComputedStyle&);

  LayoutRect BoundsForDrawingRecorder(const PaintInfo&,
                                      const LayoutPoint& adjusted_paint_offset);

  typedef Vector<const FillLayer*, 8> FillLayerOcclusionOutputList;
  // Returns true if the result fill layers have non-associative blending or
  // compositing mode.  (i.e. The rendering will be different without creating
  // isolation group by context.saveLayer().) Note that the output list will be
  // in top-bottom order.
  bool CalculateFillLayerOcclusionCulling(
      FillLayerOcclusionOutputList& reversed_paint_list,
      const FillLayer&,
      const Document&,
      const ComputedStyle&);

  struct FillLayerInfo {
    STACK_ALLOCATED();

    FillLayerInfo(const Document&,
                  const ComputedStyle&,
                  bool has_overflow_clip,
                  Color bg_color,
                  const FillLayer&,
                  BackgroundBleedAvoidance,
                  bool include_left_edge,
                  bool include_right_edge);

    // FillLayerInfo is a temporary, stack-allocated container which cannot
    // outlive the StyleImage.  This would normally be a raw pointer, if not for
    // the Oilpan tooling complaints.
    Member<StyleImage> image;
    Color color;

    bool include_left_edge;
    bool include_right_edge;
    bool is_bottom_layer;
    bool is_border_fill;
    bool is_clipped_with_local_scrolling;
    bool is_rounded_fill;
    bool should_paint_image;
    bool should_paint_color;
  };

  static FloatRoundedRect GetBackgroundRoundedRect(
      const ComputedStyle&,
      const LayoutRect& border_rect,
      bool has_line_box_sibling,
      const LayoutSize& inline_box_size,
      bool include_logical_left_edge,
      bool include_logical_right_edge);
  static FloatRoundedRect BackgroundRoundedRectAdjustedForBleedAvoidance(
      const ComputedStyle&,
      const LayoutRect& border_rect,
      BackgroundBleedAvoidance,
      bool has_line_box_sibling,
      const LayoutSize& box_size,
      bool include_logical_left_edge,
      bool include_logical_right_edge);
  static FloatRoundedRect RoundedBorderRectForClip(
      const ComputedStyle&,
      const FillLayerInfo,
      const FillLayer&,
      const LayoutRect&,
      BackgroundBleedAvoidance,
      bool has_line_box_sibling,
      const LayoutSize&,
      LayoutRectOutsets border_padding_insets);
};

}  // namespace blink

#endif
