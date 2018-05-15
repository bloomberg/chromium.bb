// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_BOX_PAINTER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_BOX_PAINTER_BASE_H_

#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/style/shadow_data.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class Color;
class FillLayer;
class IntRect;
class LayoutPoint;
class LayoutRect;
struct PaintInfo;
class ComputedStyle;

// Common base class for InlineFlowBoxPainter and NGInlineBoxFragmentPainter.
// Implements layout agnostic inline box painting behavior.
class InlineBoxPainterBase {
  STACK_ALLOCATED();

 public:
  InlineBoxPainterBase(const ImageResourceObserver& image_observer,
                       const Document* document,
                       Node* node,
                       const ComputedStyle& style,
                       const ComputedStyle& line_style)
      : image_observer_(image_observer),
        document_(document),
        node_(node),
        style_(style),
        line_style_(line_style) {}

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint& paint_offset,
                                    LayoutRect adjusted_frame_rect,
                                    BackgroundImageGeometry,
                                    bool include_logical_left_edge,
                                    bool include_logical_right_edge);

 protected:
  void PaintFillLayers(const PaintInfo&,
                       const Color&,
                       const FillLayer&,
                       const LayoutRect&,
                       BackgroundImageGeometry& geometry,
                       SkBlendMode op = SkBlendMode::kSrcOver);
  void PaintFillLayer(const PaintInfo&,
                      const Color&,
                      const FillLayer&,
                      const LayoutRect&,
                      BackgroundImageGeometry& geometry,
                      SkBlendMode op);
  void PaintNormalBoxShadow(const PaintInfo&,
                            const ComputedStyle&,
                            const LayoutRect& paint_rect);
  void PaintInsetBoxShadow(const PaintInfo&,
                           const ComputedStyle&,
                           const LayoutRect& paint_rect);

  virtual LayoutRect PaintRectForImageStrip(const LayoutRect&,
                                            TextDirection direction) const = 0;
  virtual bool InlineBoxHasMultipleFragments() const = 0;
  virtual bool IncludeLogicalLeftEdgeForBoxShadow() const = 0;
  virtual bool IncludeLogicalRightEdgeForBoxShadow() const = 0;

  enum BorderPaintingType {
    kDontPaintBorders,
    kPaintBordersWithoutClip,
    kPaintBordersWithClip
  };
  virtual BorderPaintingType GetBorderPaintType(
      const LayoutRect& adjusted_frame_rect,
      IntRect& adjusted_clip_rect) const = 0;

  // FIXME(eae): Make const
  virtual BoxPainterBase& BoxPainter() = 0;

  const ImageResourceObserver& image_observer_;
  Member<const Document> document_;
  Member<Node> node_;

  // Style for the corresponding node.
  const ComputedStyle& style_;

  // Style taking ::first-line into account.
  const ComputedStyle& line_style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_INLINE_BOX_PAINTER_BASE_H_
