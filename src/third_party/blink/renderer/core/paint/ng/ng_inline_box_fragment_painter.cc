// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"

#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

NGInlineBoxFragmentPainter::NGInlineBoxFragmentPainter(
    const NGPaintFragment& inline_box_fragment)
    : InlineBoxPainterBase(
          inline_box_fragment,
          &inline_box_fragment.GetLayoutObject()->GetDocument(),
          inline_box_fragment.GetLayoutObject()->GeneratingNode(),
          inline_box_fragment.Style(),
          // TODO(layout-dev): Should be first-line style.
          inline_box_fragment.Style()),
      inline_box_fragment_(inline_box_fragment),
      border_edges_(NGBorderEdges::FromPhysical(
          inline_box_fragment.PhysicalFragment().BorderEdges(),
          inline_box_fragment.Style().GetWritingMode())) {
}

void NGInlineBoxFragmentPainter::Paint(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  const LayoutPoint adjusted_paint_offset =
      paint_offset + inline_box_fragment_.Offset().ToLayoutPoint();
  if (paint_info.phase == PaintPhase::kForeground)
    PaintBackgroundBorderShadow(paint_info, adjusted_paint_offset);

  NGBoxFragmentPainter box_painter(inline_box_fragment_);
  box_painter.PaintObject(paint_info, adjusted_paint_offset, true);
}

void NGInlineBoxFragmentPainter::PaintBackgroundBorderShadow(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  DCHECK(paint_info.phase == PaintPhase::kForeground);
  if (inline_box_fragment_.Style().Visibility() != EVisibility::kVisible)
    return;

  // You can use p::first-line to specify a background. If so, the direct child
  // inline boxes of line boxes may actually have to paint a background.
  // TODO(layout-dev): Cache HasBoxDecorationBackground on the fragment like
  // we do for LayoutObject. Querying Style each time is too costly.
  bool should_paint_box_decoration_background =
      inline_box_fragment_.GetLayoutObject()->HasBoxDecorationBackground() ||
      inline_box_fragment_.PhysicalFragment().UsesFirstLineStyle();

  if (!should_paint_box_decoration_background)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, inline_box_fragment_,
          DisplayItem::kBoxDecorationBackground))
    return;

  DrawingRecorder recorder(paint_info.context, inline_box_fragment_,
                           DisplayItem::kBoxDecorationBackground);

  LayoutRect frame_rect =
      inline_box_fragment_.PhysicalFragment().LocalRect().ToLayoutRect();
  LayoutPoint adjusted_paint_offset = paint_offset;

  LayoutRect adjusted_frame_rect =
      LayoutRect(adjusted_paint_offset, frame_rect.Size());

  NGPaintFragment::FragmentRange fragments =
      inline_box_fragment_.InlineFragmentsFor(
          inline_box_fragment_.GetLayoutObject());
  NGPaintFragment::FragmentRange::iterator iter = fragments.begin();
  bool object_has_multiple_boxes = ++iter != fragments.end();

  // TODO(eae): Switch to LayoutNG version of BackgroundImageGeometry.
  BackgroundImageGeometry geometry(*static_cast<const LayoutBoxModelObject*>(
      inline_box_fragment_.GetLayoutObject()));
  NGBoxFragmentPainter box_painter(inline_box_fragment_);
  PaintBoxDecorationBackground(
      box_painter, paint_info, paint_offset, adjusted_frame_rect, geometry,
      object_has_multiple_boxes, border_edges_.line_left,
      border_edges_.line_right);
}

void NGInlineBoxFragmentPainter::ComputeFragmentOffsetOnLine(
    TextDirection direction,
    LayoutUnit* offset_on_line,
    LayoutUnit* total_width) const {
  WritingMode writing_mode = inline_box_fragment_.Style().GetWritingMode();
  NGPaintFragment::FragmentRange fragments =
      inline_box_fragment_.InlineFragmentsFor(
          inline_box_fragment_.GetLayoutObject());

  LayoutUnit before;
  LayoutUnit after;
  bool before_self = true;
  for (auto iter = fragments.begin(); iter != fragments.end(); ++iter) {
    if (*iter == &inline_box_fragment_) {
      before_self = false;
      continue;
    }
    if (before_self)
      before += NGFragment(writing_mode, iter->PhysicalFragment()).InlineSize();
    else
      after += NGFragment(writing_mode, iter->PhysicalFragment()).InlineSize();
  }

  NGFragment logical_fragment(writing_mode,
                              inline_box_fragment_.PhysicalFragment());
  *total_width = before + after + logical_fragment.InlineSize();

  // We're iterating over the fragments in physical order before so we need to
  // swap before and after for RTL.
  *offset_on_line = direction == TextDirection::kLtr ? before : after;
}

LayoutRect NGInlineBoxFragmentPainter::PaintRectForImageStrip(
    const LayoutRect& paint_rect,
    TextDirection direction) const {
  // We have a fill/border/mask image that spans multiple lines.
  // We need to adjust the offset by the width of all previous lines.
  // Think of background painting on inlines as though you had one long line, a
  // single continuous strip. Even though that strip has been broken up across
  // multiple lines, you still paint it as though you had one single line. This
  // means each line has to pick up the background where the previous line left
  // off.
  LayoutUnit offset_on_line;
  LayoutUnit total_width;
  ComputeFragmentOffsetOnLine(direction, &offset_on_line, &total_width);

  if (inline_box_fragment_.Style().IsHorizontalWritingMode()) {
    return LayoutRect(paint_rect.X() - offset_on_line, paint_rect.Y(),
                      total_width, paint_rect.Height());
  }
  return LayoutRect(paint_rect.X(), paint_rect.Y() - offset_on_line,
                    paint_rect.Width(), total_width);
}

static LayoutRect NGClipRectForNinePieceImageStrip(
    const ComputedStyle& style,
    const NGBorderEdges& border_edges,
    const NinePieceImage& image,
    const LayoutRect& paint_rect) {
  LayoutRect clip_rect(paint_rect);
  LayoutRectOutsets outsets = style.ImageOutsets(image);
  if (style.IsHorizontalWritingMode()) {
    clip_rect.SetY(paint_rect.Y() - outsets.Top());
    clip_rect.SetHeight(paint_rect.Height() + outsets.Top() + outsets.Bottom());
    if (border_edges.line_left) {
      clip_rect.SetX(paint_rect.X() - outsets.Left());
      clip_rect.SetWidth(paint_rect.Width() + outsets.Left());
    }
    if (border_edges.line_right)
      clip_rect.SetWidth(clip_rect.Width() + outsets.Right());
  } else {
    clip_rect.SetX(paint_rect.X() - outsets.Left());
    clip_rect.SetWidth(paint_rect.Width() + outsets.Left() + outsets.Right());
    if (border_edges.line_left) {
      clip_rect.SetY(paint_rect.Y() - outsets.Top());
      clip_rect.SetHeight(paint_rect.Height() + outsets.Top());
    }
    if (border_edges.line_right)
      clip_rect.SetHeight(clip_rect.Height() + outsets.Bottom());
  }
  return clip_rect;
}

InlineBoxPainterBase::BorderPaintingType
NGInlineBoxFragmentPainter::GetBorderPaintType(
    const LayoutRect& adjusted_frame_rect,
    IntRect& adjusted_clip_rect,
    bool object_has_multiple_boxes) const {
  adjusted_clip_rect = PixelSnappedIntRect(adjusted_frame_rect);
  if (inline_box_fragment_.Parent() &&
      inline_box_fragment_.Style().HasBorderDecoration()) {
    const NinePieceImage& border_image =
        inline_box_fragment_.Style().BorderImage();
    StyleImage* border_image_source = border_image.GetImage();
    bool has_border_image =
        border_image_source && border_image_source->CanRender();
    if (has_border_image && !border_image_source->IsLoaded())
      return kDontPaintBorders;

    // The simple case is where we either have no border image or we are the
    // only box for this object.  In those cases only a single call to draw is
    // required.
    if (!has_border_image || !object_has_multiple_boxes)
      return kPaintBordersWithoutClip;

    // We have a border image that spans multiple lines.
    adjusted_clip_rect = PixelSnappedIntRect(NGClipRectForNinePieceImageStrip(
        inline_box_fragment_.Style(), border_edges_, border_image,
        adjusted_frame_rect));
    return kPaintBordersWithClip;
  }
  return kDontPaintBorders;
}

void NGInlineBoxFragmentPainter::PaintNormalBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const LayoutRect& paint_rect) {
  BoxPainterBase::PaintNormalBoxShadow(
      info, paint_rect, s, border_edges_.line_left, border_edges_.line_right);
}

void NGInlineBoxFragmentPainter::PaintInsetBoxShadow(
    const PaintInfo& info,
    const ComputedStyle& s,
    const LayoutRect& paint_rect) {
  BoxPainterBase::PaintInsetBoxShadowWithBorderRect(
      info, paint_rect, s, border_edges_.line_left, border_edges_.line_right);
}

}  // namespace blink
