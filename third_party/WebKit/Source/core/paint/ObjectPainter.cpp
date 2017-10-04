// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPainter.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/paint/BoxBorderPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"

namespace blink {

namespace {

void PaintSingleRectangleOutline(const PaintInfo& paint_info,
                                 const IntRect& rect,
                                 const ComputedStyle& style,
                                 const Color& color) {
  DCHECK(!style.OutlineStyleIsAuto());

  LayoutRect inner(rect);
  inner.Inflate(style.OutlineOffset());
  LayoutRect outer(inner);
  outer.Inflate(style.OutlineWidth());
  const BorderEdge common_edge_info(style.OutlineWidth(), color,
                                    style.OutlineStyle());
  BoxBorderPainter(style, outer, inner, common_edge_info)
      .PaintBorder(paint_info, outer);
}

struct OutlineEdgeInfo {
  int x1;
  int y1;
  int x2;
  int y2;
  BoxSide side;
};

// Adjust length of edges if needed. Returns the width of the joint.
int AdjustJoint(int outline_width,
                OutlineEdgeInfo& edge1,
                OutlineEdgeInfo& edge2) {
  // A clockwise joint:
  // - needs no adjustment of edge length because our edges are along the
  //   clockwise outer edge of the outline;
  // - needs a positive adjacent joint width (required by drawLineForBoxSide).
  // A counterclockwise joint:
  // - needs to increase the edge length to include the joint;
  // - needs a negative adjacent joint width (required by drawLineForBoxSide).
  switch (edge1.side) {
    case kBSTop:
      switch (edge2.side) {
        case kBSRight:  // Clockwise
          return outline_width;
        case kBSLeft:  // Counterclockwise
          edge1.x2 += outline_width;
          edge2.y2 += outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    case kBSRight:
      switch (edge2.side) {
        case kBSBottom:  // Clockwise
          return outline_width;
        case kBSTop:  // Counterclockwise
          edge1.y2 += outline_width;
          edge2.x1 -= outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    case kBSBottom:
      switch (edge2.side) {
        case kBSLeft:  // Clockwise
          return outline_width;
        case kBSRight:  // Counterclockwise
          edge1.x1 -= outline_width;
          edge2.y1 -= outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    case kBSLeft:
      switch (edge2.side) {
        case kBSTop:  // Clockwise
          return outline_width;
        case kBSBottom:  // Counterclockwise
          edge1.y1 -= outline_width;
          edge2.x2 += outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    default:
      NOTREACHED();
      return 0;
  }
}

void PaintComplexOutline(GraphicsContext& graphics_context,
                         const Vector<IntRect> rects,
                         const ComputedStyle& style,
                         const Color& color) {
  DCHECK(!style.OutlineStyleIsAuto());

  // Construct a clockwise path along the outer edge of the outline.
  SkRegion region;
  int width = style.OutlineWidth();
  int outset = style.OutlineOffset() + style.OutlineWidth();
  for (auto& r : rects) {
    IntRect rect = r;
    rect.Inflate(outset);
    region.op(rect, SkRegion::kUnion_Op);
  }
  SkPath path;
  if (!region.getBoundaryPath(&path))
    return;

  Vector<OutlineEdgeInfo, 4> edges;

  SkPath::Iter iter(path, false);
  SkPoint points[4];
  size_t count = 0;
  for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb;
       verb = iter.next(points, false)) {
    if (verb != SkPath::kLine_Verb)
      continue;

    edges.Grow(++count);
    OutlineEdgeInfo& edge = edges.back();
    edge.x1 = SkScalarTruncToInt(points[0].x());
    edge.y1 = SkScalarTruncToInt(points[0].y());
    edge.x2 = SkScalarTruncToInt(points[1].x());
    edge.y2 = SkScalarTruncToInt(points[1].y());
    if (edge.x1 == edge.x2) {
      if (edge.y1 < edge.y2) {
        edge.x1 -= width;
        edge.side = kBSRight;
      } else {
        std::swap(edge.y1, edge.y2);
        edge.x2 += width;
        edge.side = kBSLeft;
      }
    } else {
      DCHECK(edge.y1 == edge.y2);
      if (edge.x1 < edge.x2) {
        edge.y2 += width;
        edge.side = kBSTop;
      } else {
        std::swap(edge.x1, edge.x2);
        edge.y1 -= width;
        edge.side = kBSBottom;
      }
    }
  }

  if (!count)
    return;

  Color outline_color = color;
  bool use_transparency_layer = color.HasAlpha();
  if (use_transparency_layer) {
    graphics_context.BeginLayer(static_cast<float>(color.Alpha()) / 255);
    outline_color =
        Color(outline_color.Red(), outline_color.Green(), outline_color.Blue());
  }

  DCHECK(count >= 4 && edges.size() == count);
  int first_adjacent_width = AdjustJoint(width, edges.back(), edges.front());

  // The width of the angled part of starting and ending joint of the current
  // edge.
  int adjacent_width_start = first_adjacent_width;
  int adjacent_width_end;
  for (size_t i = 0; i < count; ++i) {
    OutlineEdgeInfo& edge = edges[i];
    adjacent_width_end = i == count - 1
                             ? first_adjacent_width
                             : AdjustJoint(width, edge, edges[i + 1]);
    int adjacent_width1 = adjacent_width_start;
    int adjacent_width2 = adjacent_width_end;
    if (edge.side == kBSLeft || edge.side == kBSBottom)
      std::swap(adjacent_width1, adjacent_width2);
    ObjectPainter::DrawLineForBoxSide(graphics_context, edge.x1, edge.y1,
                                      edge.x2, edge.y2, edge.side,
                                      outline_color, style.OutlineStyle(),
                                      adjacent_width1, adjacent_width2, false);
    adjacent_width_start = adjacent_width_end;
  }

  if (use_transparency_layer)
    graphics_context.EndLayer();
}

void FillQuad(GraphicsContext& context,
              const FloatPoint quad[],
              const Color& color,
              bool antialias) {
  SkPath path;
  path.moveTo(quad[0]);
  path.lineTo(quad[1]);
  path.lineTo(quad[2]);
  path.lineTo(quad[3]);
  PaintFlags flags(context.FillFlags());
  flags.setAntiAlias(antialias);
  flags.setColor(color.Rgb());

  context.DrawPath(path, flags);
}

}  // namespace

void ObjectPainter::PaintOutline(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const ComputedStyle& style_to_use = layout_object_.StyleRef();
  if (!style_to_use.HasOutline() ||
      style_to_use.Visibility() != EVisibility::kVisible)
    return;

  // Only paint the focus ring by hand if the theme isn't able to draw the focus
  // ring.
  if (style_to_use.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(
          layout_object_.GetNode(), style_to_use)) {
    return;
  }

  Vector<LayoutRect> outline_rects;
  layout_object_.AddOutlineRects(
      outline_rects, paint_offset,
      layout_object_.OutlineRectsShouldIncludeBlockVisualOverflow());
  if (outline_rects.IsEmpty())
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_object_, paint_info.phase))
    return;

  // The result rects are in coordinates of m_layoutObject's border box.
  // Block flipping is not applied yet if !m_layoutObject.isBox().
  if (!layout_object_.IsBox() &&
      layout_object_.StyleRef().IsFlippedBlocksWritingMode()) {
    LayoutBlock* container = layout_object_.ContainingBlock();
    if (container) {
      layout_object_.LocalToAncestorRects(outline_rects, container,
                                          -paint_offset, paint_offset);
      if (outline_rects.IsEmpty())
        return;
    }
  }

  Vector<IntRect> pixel_snapped_outline_rects;
  for (auto& r : outline_rects)
    pixel_snapped_outline_rects.push_back(PixelSnappedIntRect(r));

  IntRect united_outline_rect =
      UnionRectEvenIfEmpty(pixel_snapped_outline_rects);
  IntRect bounds = united_outline_rect;
  bounds.Inflate(layout_object_.StyleRef().OutlineOutsetExtent());
  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_object_,
                                       paint_info.phase, bounds);

  Color color =
      layout_object_.ResolveColor(style_to_use, CSSPropertyOutlineColor);
  if (style_to_use.OutlineStyleIsAuto()) {
    paint_info.context.DrawFocusRing(
        pixel_snapped_outline_rects,
        style_to_use.GetOutlineStrokeWidthForFocusRing(),
        style_to_use.OutlineOffset(), color);
    return;
  }

  if (united_outline_rect == pixel_snapped_outline_rects[0]) {
    PaintSingleRectangleOutline(paint_info, united_outline_rect, style_to_use,
                                color);
    return;
  }
  PaintComplexOutline(paint_info.context, pixel_snapped_outline_rects,
                      style_to_use, color);
}

void ObjectPainter::PaintInlineChildrenOutlines(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintDescendantOutlines(paint_info.phase));

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  for (LayoutObject* child = layout_object_.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutInline() &&
        !ToLayoutInline(child)->HasSelfPaintingLayer())
      child->Paint(paint_info_for_descendants, paint_offset);
  }
}

void ObjectPainter::AddPDFURLRectIfNeeded(const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  DCHECK(paint_info.IsPrinting());
  if (layout_object_.IsElementContinuation() || !layout_object_.GetNode() ||
      !layout_object_.GetNode()->IsLink() ||
      layout_object_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  KURL url = ToElement(layout_object_.GetNode())->HrefURL();
  if (!url.IsValid())
    return;

  Vector<LayoutRect> visual_overflow_rects;
  layout_object_.AddElementVisualOverflowRects(visual_overflow_rects,
                                               paint_offset);
  IntRect rect = PixelSnappedIntRect(UnionRect(visual_overflow_rects));
  if (rect.IsEmpty())
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_object_,
          DisplayItem::kPrintedContentPDFURLRect))
    return;

  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_object_,
                                       DisplayItem::kPrintedContentPDFURLRect,
                                       rect);
  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url,
                                      layout_object_.GetDocument().BaseURL())) {
    String fragment_name = url.FragmentIdentifier();
    if (layout_object_.GetDocument().FindAnchor(fragment_name))
      paint_info.context.SetURLFragmentForRect(fragment_name, rect);
    return;
  }
  paint_info.context.SetURLForRect(url, rect);
}

void ObjectPainter::DrawLineForBoxSide(GraphicsContext& graphics_context,
                                       float x1,
                                       float y1,
                                       float x2,
                                       float y2,
                                       BoxSide side,
                                       Color color,
                                       EBorderStyle style,
                                       int adjacent_width1,
                                       int adjacent_width2,
                                       bool antialias) {
  float thickness;
  float length;
  if (side == kBSTop || side == kBSBottom) {
    thickness = y2 - y1;
    length = x2 - x1;
  } else {
    thickness = x2 - x1;
    length = y2 - y1;
  }

  // We would like this check to be an ASSERT as we don't want to draw empty
  // borders. However nothing guarantees that the following recursive calls to
  // drawLineForBoxSide will have positive thickness and length.
  if (length <= 0 || thickness <= 0)
    return;

  if (style == EBorderStyle::kDouble && thickness < 3)
    style = EBorderStyle::kSolid;

  switch (style) {
    case EBorderStyle::kNone:
    case EBorderStyle::kHidden:
      return;
    case EBorderStyle::kDotted:
    case EBorderStyle::kDashed:
      DrawDashedOrDottedBoxSide(graphics_context, x1, y1, x2, y2, side, color,
                                thickness, style, antialias);
      break;
    case EBorderStyle::kDouble:
      DrawDoubleBoxSide(graphics_context, x1, y1, x2, y2, length, side, color,
                        thickness, adjacent_width1, adjacent_width2, antialias);
      break;
    case EBorderStyle::kRidge:
    case EBorderStyle::kGroove:
      DrawRidgeOrGrooveBoxSide(graphics_context, x1, y1, x2, y2, side, color,
                               style, adjacent_width1, adjacent_width2,
                               antialias);
      break;
    case EBorderStyle::kInset:
      // FIXME: Maybe we should lighten the colors on one side like Firefox.
      // https://bugs.webkit.org/show_bug.cgi?id=58608
      if (side == kBSTop || side == kBSLeft)
        color = color.Dark();
    // fall through
    case EBorderStyle::kOutset:
      if (style == EBorderStyle::kOutset &&
          (side == kBSBottom || side == kBSRight))
        color = color.Dark();
    // fall through
    case EBorderStyle::kSolid:
      DrawSolidBoxSide(graphics_context, x1, y1, x2, y2, side, color,
                       adjacent_width1, adjacent_width2, antialias);
      break;
  }
}

void ObjectPainter::DrawDashedOrDottedBoxSide(GraphicsContext& graphics_context,
                                              int x1,
                                              int y1,
                                              int x2,
                                              int y2,
                                              BoxSide side,
                                              Color color,
                                              int thickness,
                                              EBorderStyle style,
                                              bool antialias) {
  DCHECK_GT(thickness, 0);

  GraphicsContextStateSaver state_saver(graphics_context);
  graphics_context.SetShouldAntialias(antialias);
  graphics_context.SetStrokeColor(color);
  graphics_context.SetStrokeThickness(thickness);
  graphics_context.SetStrokeStyle(
      style == EBorderStyle::kDashed ? kDashedStroke : kDottedStroke);

  switch (side) {
    case kBSBottom:
    case kBSTop: {
      int mid_y = y1 + thickness / 2;
      graphics_context.DrawLine(IntPoint(x1, mid_y), IntPoint(x2, mid_y));
      break;
    }
    case kBSRight:
    case kBSLeft: {
      int mid_x = x1 + thickness / 2;
      graphics_context.DrawLine(IntPoint(mid_x, y1), IntPoint(mid_x, y2));
      break;
    }
  }
}

void ObjectPainter::DrawDoubleBoxSide(GraphicsContext& graphics_context,
                                      int x1,
                                      int y1,
                                      int x2,
                                      int y2,
                                      int length,
                                      BoxSide side,
                                      Color color,
                                      float thickness,
                                      int adjacent_width1,
                                      int adjacent_width2,
                                      bool antialias) {
  int third_of_thickness = (thickness + 1) / 3;
  DCHECK_GT(third_of_thickness, 0);

  if (!adjacent_width1 && !adjacent_width2) {
    StrokeStyle old_stroke_style = graphics_context.GetStrokeStyle();
    graphics_context.SetStrokeStyle(kNoStroke);
    graphics_context.SetFillColor(color);

    bool was_antialiased = graphics_context.ShouldAntialias();
    graphics_context.SetShouldAntialias(antialias);

    switch (side) {
      case kBSTop:
      case kBSBottom:
        graphics_context.DrawRect(IntRect(x1, y1, length, third_of_thickness));
        graphics_context.DrawRect(
            IntRect(x1, y2 - third_of_thickness, length, third_of_thickness));
        break;
      case kBSLeft:
      case kBSRight:
        graphics_context.DrawRect(IntRect(x1, y1, third_of_thickness, length));
        graphics_context.DrawRect(
            IntRect(x2 - third_of_thickness, y1, third_of_thickness, length));
        break;
    }

    graphics_context.SetShouldAntialias(was_antialiased);
    graphics_context.SetStrokeStyle(old_stroke_style);
    return;
  }

  int adjacent1_big_third =
      ((adjacent_width1 > 0) ? adjacent_width1 + 1 : adjacent_width1 - 1) / 3;
  int adjacent2_big_third =
      ((adjacent_width2 > 0) ? adjacent_width2 + 1 : adjacent_width2 - 1) / 3;

  switch (side) {
    case kBSTop:
      DrawLineForBoxSide(
          graphics_context, x1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
          y1, x2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0),
          y1 + third_of_thickness, side, color, EBorderStyle::kSolid,
          adjacent1_big_third, adjacent2_big_third, antialias);
      DrawLineForBoxSide(graphics_context,
                         x1 + std::max((adjacent_width1 * 2 + 1) / 3, 0),
                         y2 - third_of_thickness,
                         x2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), y2,
                         side, color, EBorderStyle::kSolid, adjacent1_big_third,
                         adjacent2_big_third, antialias);
      break;
    case kBSLeft:
      DrawLineForBoxSide(graphics_context, x1,
                         y1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
                         x1 + third_of_thickness,
                         y2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), side,
                         color, EBorderStyle::kSolid, adjacent1_big_third,
                         adjacent2_big_third, antialias);
      DrawLineForBoxSide(graphics_context, x2 - third_of_thickness,
                         y1 + std::max((adjacent_width1 * 2 + 1) / 3, 0), x2,
                         y2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), side,
                         color, EBorderStyle::kSolid, adjacent1_big_third,
                         adjacent2_big_third, antialias);
      break;
    case kBSBottom:
      DrawLineForBoxSide(
          graphics_context, x1 + std::max((adjacent_width1 * 2 + 1) / 3, 0), y1,
          x2 - std::max((adjacent_width2 * 2 + 1) / 3, 0),
          y1 + third_of_thickness, side, color, EBorderStyle::kSolid,
          adjacent1_big_third, adjacent2_big_third, antialias);
      DrawLineForBoxSide(graphics_context,
                         x1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
                         y2 - third_of_thickness,
                         x2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), y2,
                         side, color, EBorderStyle::kSolid, adjacent1_big_third,
                         adjacent2_big_third, antialias);
      break;
    case kBSRight:
      DrawLineForBoxSide(graphics_context, x1,
                         y1 + std::max((adjacent_width1 * 2 + 1) / 3, 0),
                         x1 + third_of_thickness,
                         y2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), side,
                         color, EBorderStyle::kSolid, adjacent1_big_third,
                         adjacent2_big_third, antialias);
      DrawLineForBoxSide(graphics_context, x2 - third_of_thickness,
                         y1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0), x2,
                         y2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), side,
                         color, EBorderStyle::kSolid, adjacent1_big_third,
                         adjacent2_big_third, antialias);
      break;
    default:
      break;
  }
}

void ObjectPainter::DrawRidgeOrGrooveBoxSide(GraphicsContext& graphics_context,
                                             int x1,
                                             int y1,
                                             int x2,
                                             int y2,
                                             BoxSide side,
                                             Color color,
                                             EBorderStyle style,
                                             int adjacent_width1,
                                             int adjacent_width2,
                                             bool antialias) {
  EBorderStyle s1;
  EBorderStyle s2;
  if (style == EBorderStyle::kGroove) {
    s1 = EBorderStyle::kInset;
    s2 = EBorderStyle::kOutset;
  } else {
    s1 = EBorderStyle::kOutset;
    s2 = EBorderStyle::kInset;
  }

  int adjacent1_big_half =
      ((adjacent_width1 > 0) ? adjacent_width1 + 1 : adjacent_width1 - 1) / 2;
  int adjacent2_big_half =
      ((adjacent_width2 > 0) ? adjacent_width2 + 1 : adjacent_width2 - 1) / 2;

  switch (side) {
    case kBSTop:
      DrawLineForBoxSide(
          graphics_context, x1 + std::max(-adjacent_width1, 0) / 2, y1,
          x2 - std::max(-adjacent_width2, 0) / 2, (y1 + y2 + 1) / 2, side,
          color, s1, adjacent1_big_half, adjacent2_big_half, antialias);
      DrawLineForBoxSide(
          graphics_context, x1 + std::max(adjacent_width1 + 1, 0) / 2,
          (y1 + y2 + 1) / 2, x2 - std::max(adjacent_width2 + 1, 0) / 2, y2,
          side, color, s2, adjacent_width1 / 2, adjacent_width2 / 2, antialias);
      break;
    case kBSLeft:
      DrawLineForBoxSide(
          graphics_context, x1, y1 + std::max(-adjacent_width1, 0) / 2,
          (x1 + x2 + 1) / 2, y2 - std::max(-adjacent_width2, 0) / 2, side,
          color, s1, adjacent1_big_half, adjacent2_big_half, antialias);
      DrawLineForBoxSide(graphics_context, (x1 + x2 + 1) / 2,
                         y1 + std::max(adjacent_width1 + 1, 0) / 2, x2,
                         y2 - std::max(adjacent_width2 + 1, 0) / 2, side, color,
                         s2, adjacent_width1 / 2, adjacent_width2 / 2,
                         antialias);
      break;
    case kBSBottom:
      DrawLineForBoxSide(
          graphics_context, x1 + std::max(adjacent_width1, 0) / 2, y1,
          x2 - std::max(adjacent_width2, 0) / 2, (y1 + y2 + 1) / 2, side, color,
          s2, adjacent1_big_half, adjacent2_big_half, antialias);
      DrawLineForBoxSide(
          graphics_context, x1 + std::max(-adjacent_width1 + 1, 0) / 2,
          (y1 + y2 + 1) / 2, x2 - std::max(-adjacent_width2 + 1, 0) / 2, y2,
          side, color, s1, adjacent_width1 / 2, adjacent_width2 / 2, antialias);
      break;
    case kBSRight:
      DrawLineForBoxSide(
          graphics_context, x1, y1 + std::max(adjacent_width1, 0) / 2,
          (x1 + x2 + 1) / 2, y2 - std::max(adjacent_width2, 0) / 2, side, color,
          s2, adjacent1_big_half, adjacent2_big_half, antialias);
      DrawLineForBoxSide(graphics_context, (x1 + x2 + 1) / 2,
                         y1 + std::max(-adjacent_width1 + 1, 0) / 2, x2,
                         y2 - std::max(-adjacent_width2 + 1, 0) / 2, side,
                         color, s1, adjacent_width1 / 2, adjacent_width2 / 2,
                         antialias);
      break;
  }
}

void ObjectPainter::DrawSolidBoxSide(GraphicsContext& graphics_context,
                                     int x1,
                                     int y1,
                                     int x2,
                                     int y2,
                                     BoxSide side,
                                     Color color,
                                     int adjacent_width1,
                                     int adjacent_width2,
                                     bool antialias) {
  DCHECK_GE(x2, x1);
  DCHECK_GE(y2, y1);

  if (!adjacent_width1 && !adjacent_width2) {
    // Tweak antialiasing to match the behavior of fillQuad();
    // this matters for rects in transformed contexts.
    bool was_antialiased = graphics_context.ShouldAntialias();
    if (antialias != was_antialiased)
      graphics_context.SetShouldAntialias(antialias);
    graphics_context.FillRect(IntRect(x1, y1, x2 - x1, y2 - y1), color);
    if (antialias != was_antialiased)
      graphics_context.SetShouldAntialias(was_antialiased);
    return;
  }

  FloatPoint quad[4];
  switch (side) {
    case kBSTop:
      quad[0] = FloatPoint(x1 + std::max(-adjacent_width1, 0), y1);
      quad[1] = FloatPoint(x1 + std::max(adjacent_width1, 0), y2);
      quad[2] = FloatPoint(x2 - std::max(adjacent_width2, 0), y2);
      quad[3] = FloatPoint(x2 - std::max(-adjacent_width2, 0), y1);
      break;
    case kBSBottom:
      quad[0] = FloatPoint(x1 + std::max(adjacent_width1, 0), y1);
      quad[1] = FloatPoint(x1 + std::max(-adjacent_width1, 0), y2);
      quad[2] = FloatPoint(x2 - std::max(-adjacent_width2, 0), y2);
      quad[3] = FloatPoint(x2 - std::max(adjacent_width2, 0), y1);
      break;
    case kBSLeft:
      quad[0] = FloatPoint(x1, y1 + std::max(-adjacent_width1, 0));
      quad[1] = FloatPoint(x1, y2 - std::max(-adjacent_width2, 0));
      quad[2] = FloatPoint(x2, y2 - std::max(adjacent_width2, 0));
      quad[3] = FloatPoint(x2, y1 + std::max(adjacent_width1, 0));
      break;
    case kBSRight:
      quad[0] = FloatPoint(x1, y1 + std::max(adjacent_width1, 0));
      quad[1] = FloatPoint(x1, y2 - std::max(adjacent_width2, 0));
      quad[2] = FloatPoint(x2, y2 - std::max(-adjacent_width2, 0));
      quad[3] = FloatPoint(x2, y1 + std::max(-adjacent_width1, 0));
      break;
  }

  FillQuad(graphics_context, quad, color, antialias);
}

void ObjectPainter::PaintAllPhasesAtomically(const PaintInfo& paint_info,
                                             const LayoutPoint& paint_offset) {
  // Pass PaintPhaseSelection and PaintPhaseTextClip to the descendants so that
  // they will paint for selection and text clip respectively. We don't need
  // complete painting for these phases.
  if (paint_info.phase == kPaintPhaseSelection ||
      paint_info.phase == kPaintPhaseTextClip) {
    layout_object_.Paint(paint_info, paint_offset);
    return;
  }

  if (paint_info.phase != kPaintPhaseForeground)
    return;

  PaintInfo info(paint_info);
  info.phase = kPaintPhaseBlockBackground;
  layout_object_.Paint(info, paint_offset);
  info.phase = kPaintPhaseFloat;
  layout_object_.Paint(info, paint_offset);
  info.phase = kPaintPhaseForeground;
  layout_object_.Paint(info, paint_offset);
  info.phase = kPaintPhaseOutline;
  layout_object_.Paint(info, paint_offset);
}

#if DCHECK_IS_ON()
void ObjectPainter::DoCheckPaintOffset(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  // TODO(pdr): Let painter and paint property tree builder generate the same
  // paint offset for LayoutScrollbarPart. crbug.com/664249.
  if (layout_object_.IsLayoutScrollbarPart())
    return;

  LayoutPoint adjusted_paint_offset = paint_offset;
  if (layout_object_.IsBox())
    adjusted_paint_offset += ToLayoutBox(layout_object_).Location();
  DCHECK(layout_object_.PaintOffset() == adjusted_paint_offset)
      << " Paint offset mismatch: " << layout_object_.DebugName()
      << " from PaintPropertyTreeBuilder: "
      << layout_object_.PaintOffset().ToString()
      << " from painter: " << adjusted_paint_offset.ToString();
}
#endif

}  // namespace blink
