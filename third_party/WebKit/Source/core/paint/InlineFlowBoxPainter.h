// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineFlowBoxPainter_h
#define InlineFlowBoxPainter_h

#include "core/style/ShadowData.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Color;
class FillLayer;
class InlineFlowBox;
class IntRect;
class LayoutPoint;
class LayoutRect;
class LayoutSize;
class LayoutUnit;
struct PaintInfo;
class ComputedStyle;

class InlineFlowBoxPainter {
  STACK_ALLOCATED();

 public:
  InlineFlowBoxPainter(const InlineFlowBox& inline_flow_box)
      : inline_flow_box_(inline_flow_box) {}
  void Paint(const PaintInfo&,
             const LayoutPoint& paint_offset,
             const LayoutUnit line_top,
             const LayoutUnit line_bottom);

  LayoutRect FrameRectClampedToLineTopAndBottomIfNeeded() const;

 private:
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint& paint_offset);
  void PaintMask(const PaintInfo&, const LayoutPoint& paint_offset);
  void PaintFillLayers(const PaintInfo&,
                       const Color&,
                       const FillLayer&,
                       const LayoutRect&,
                       SkBlendMode op = SkBlendMode::kSrcOver);
  void PaintFillLayer(const PaintInfo&,
                      const Color&,
                      const FillLayer&,
                      const LayoutRect&,
                      SkBlendMode op);
  inline bool ShouldForceIncludeLogicalEdges() const;
  inline bool IncludeLogicalLeftEdgeForBoxShadow() const;
  inline bool IncludeLogicalRightEdgeForBoxShadow() const;
  void PaintNormalBoxShadow(const PaintInfo&,
                            const ComputedStyle&,
                            const LayoutRect& paint_rect);
  void PaintInsetBoxShadow(const PaintInfo&,
                           const ComputedStyle&,
                           const LayoutRect& paint_rect);
  LayoutRect PaintRectForImageStrip(const LayoutPoint& paint_offset,
                                    const LayoutSize& frame_size,
                                    TextDirection) const;

  enum BorderPaintingType {
    kDontPaintBorders,
    kPaintBordersWithoutClip,
    kPaintBordersWithClip
  };
  BorderPaintingType GetBorderPaintType(const LayoutRect& adjusted_frame_rect,
                                        IntRect& adjusted_clip_rect) const;

  const InlineFlowBox& inline_flow_box_;
};

}  // namespace blink

#endif  // InlineFlowBoxPainter_h
