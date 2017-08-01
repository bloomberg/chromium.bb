// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxModelObjectPainter_h
#define BoxModelObjectPainter_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/paint/BoxPainterBase.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class FillLayer;
class InlineFlowBox;
class LayoutRect;
struct PaintInfo;
class LayoutBoxModelObject;
class Image;

// BoxModelObjectPainter is a class that can paint either a LayoutBox or a
// LayoutInline and allows code sharing between block and inline block painting.
class BoxModelObjectPainter : public BoxPainterBase {
  STACK_ALLOCATED();

 public:
  BoxModelObjectPainter(const LayoutBoxModelObject&,
                        const InlineFlowBox* = nullptr,
                        const LayoutSize& flow_box_size = LayoutSize());

  static bool IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
      const LayoutBoxModelObject*,
      const PaintInfo&);

 protected:
  BoxPainterBase::FillLayerInfo GetFillLayerInfo(
      const Color&,
      const FillLayer&,
      BackgroundBleedAvoidance) const override;

  void PaintFillLayerTextFillBox(GraphicsContext&,
                                 const BoxPainterBase::FillLayerInfo&,
                                 Image*,
                                 SkBlendMode composite_op,
                                 const BackgroundImageGeometry&,
                                 const LayoutRect&,
                                 LayoutRect scrolled_paint_rect) override;
  LayoutRect AdjustForScrolledContent(const PaintInfo&,
                                      const BoxPainterBase::FillLayerInfo&,
                                      const LayoutRect&) override;
  FloatRoundedRect GetBackgroundRoundedRect(
      const LayoutRect&,
      bool include_logical_left_edge,
      bool include_logical_right_edge) const override;

 private:
  const LayoutBoxModelObject& box_model_;
  const InlineFlowBox* flow_box_;
  const LayoutSize flow_box_size_;
};

}  // namespace blink

#endif  // BoxModelObjectPainter_h
