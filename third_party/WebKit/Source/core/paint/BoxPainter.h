// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainter_h
#define BoxPainter_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/paint/BoxPainterBase.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Optional.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace blink {

class ComputedStyle;
class FillLayer;
class GraphicsContext;
class InlineFlowBox;
class LayoutPoint;
class LayoutRect;
class NinePieceImage;
struct PaintInfo;
class LayoutBox;
class LayoutObject;
class LayoutBoxModelObject;

class BoxPainter : public BoxPainterBase {
  STACK_ALLOCATED();

 public:
  BoxPainter(const LayoutBox& layout_box) : layout_box_(layout_box) {}
  void Paint(const PaintInfo&, const LayoutPoint&);

  void PaintChildren(const PaintInfo&, const LayoutPoint&);
  void PaintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
  void PaintMask(const PaintInfo&, const LayoutPoint&);
  void PaintClippingMask(const PaintInfo&, const LayoutPoint&);

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
