// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_box_fragment_painter_h
#define ng_box_fragment_painter_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/paint/BoxPainterBase.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class FillLayer;
class LayoutRect;
struct PaintInfo;
class NGPaintFragment;
class Image;

// Painter for LayoutNG box fragments, paints borders and background. Delegates
// to NGTextFragmentPainter to paint line box fragments.
class NGBoxFragmentPainter : public BoxPainterBase {
  STACK_ALLOCATED();

 public:
  NGBoxFragmentPainter(const NGPaintFragment&);

  void Paint(const PaintInfo&, const LayoutPoint&);
  void PaintChildren(const PaintInfo&, const LayoutPoint&);

  void PaintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
  void PaintBoxDecorationBackgroundWithRect(const PaintInfo&,
                                            const LayoutPoint&,
                                            const LayoutRect&);

  static bool IsPaintingBackgroundOfPaintContainerIntoScrollingContentsLayer(
      const NGPaintFragment&,
      const PaintInfo&);

  LayoutRect BoundsForDrawingRecorder(const PaintInfo&,
                                      const LayoutPoint& adjusted_paint_offset);

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

 private:
  void PaintChildren(const Vector<std::unique_ptr<const NGPaintFragment>>&,
                     const PaintInfo&,
                     const LayoutPoint&);
  void PaintText(const NGPaintFragment&,
                 const PaintInfo&,
                 const LayoutPoint& paint_offset);
  void PaintBackground(const PaintInfo&,
                       const LayoutRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);

  const NGPaintFragment& box_fragment_;
};

}  // namespace blink

#endif  // ng_box_fragment_painter_h
