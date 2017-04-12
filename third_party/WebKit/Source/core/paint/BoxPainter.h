// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainter_h
#define BoxPainter_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/style/ShadowData.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Allocator.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink {

class ComputedStyle;
class Document;
class FillLayer;
class FloatRoundedRect;
class GraphicsContext;
class Image;
class InlineFlowBox;
class LayoutPoint;
class LayoutRect;
class LayoutBoxModelObject;
class NinePieceImage;
struct PaintInfo;
class LayoutBox;
class LayoutObject;

class BoxPainter {
  STACK_ALLOCATED();

 public:
  BoxPainter(const LayoutBox& layout_box) : layout_box_(layout_box) {}
  void Paint(const PaintInfo&, const LayoutPoint&);

  void PaintChildren(const PaintInfo&, const LayoutPoint&);
  void PaintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
  void PaintMask(const PaintInfo&, const LayoutPoint&);
  void PaintClippingMask(const PaintInfo&, const LayoutPoint&);

  typedef Vector<const FillLayer*, 8> FillLayerOcclusionOutputList;
  // Returns true if the result fill layers have non-associative blending or
  // compositing mode.  (i.e. The rendering will be different without creating
  // isolation group by context.saveLayer().) Note that the output list will be
  // in top-bottom order.
  bool CalculateFillLayerOcclusionCulling(
      FillLayerOcclusionOutputList& reversed_paint_list,
      const FillLayer&);

  // Returns true if the fill layer will certainly occlude anything painted
  // behind it.
  static bool IsFillLayerOpaque(const FillLayer&, const LayoutObject&);

  void PaintFillLayers(const PaintInfo&,
                       const Color&,
                       const FillLayer&,
                       const LayoutRect&,
                       BackgroundBleedAvoidance = kBackgroundBleedNone,
                       SkBlendMode = SkBlendMode::kSrcOver,
                       const LayoutObject* background_object = nullptr);
  void PaintMaskImages(const PaintInfo&, const LayoutRect&);
  void PaintBoxDecorationBackgroundWithRect(const PaintInfo&,
                                            const LayoutPoint&,
                                            const LayoutRect&);
  static void PaintFillLayer(const LayoutBoxModelObject&,
                             const PaintInfo&,
                             const Color&,
                             const FillLayer&,
                             const LayoutRect&,
                             BackgroundBleedAvoidance,
                             const InlineFlowBox* = nullptr,
                             const LayoutSize& = LayoutSize(),
                             SkBlendMode = SkBlendMode::kSrcOver,
                             const LayoutObject* background_object = nullptr);
  static InterpolationQuality ChooseInterpolationQuality(const LayoutObject&,
                                                         Image*,
                                                         const void*,
                                                         const LayoutSize&);
  static bool PaintNinePieceImage(const LayoutBoxModelObject&,
                                  GraphicsContext&,
                                  const LayoutRect&,
                                  const ComputedStyle&,
                                  const NinePieceImage&,
                                  SkBlendMode = SkBlendMode::kSrcOver);
  static void PaintBorder(const LayoutBoxModelObject&,
                          const PaintInfo&,
                          const LayoutRect&,
                          const ComputedStyle&,
                          BackgroundBleedAvoidance = kBackgroundBleedNone,
                          bool include_logical_left_edge = true,
                          bool include_logical_right_edge = true);
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
  static bool ShouldForceWhiteBackgroundForPrintEconomy(const ComputedStyle&,
                                                        const Document&);

  LayoutRect BoundsForDrawingRecorder(const PaintInfo&,
                                      const LayoutPoint& adjusted_paint_offset);

  static bool IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
      const LayoutBoxModelObject*,
      const PaintInfo&);

 private:
  void PaintBackground(const PaintInfo&,
                       const LayoutRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);

  const LayoutBox& layout_box_;
};

}  // namespace blink

#endif
