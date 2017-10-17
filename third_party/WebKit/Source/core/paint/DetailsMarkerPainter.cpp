// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/DetailsMarkerPainter.h"

#include "core/layout/LayoutDetailsMarker.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/Path.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void DetailsMarkerPainter::Paint(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_details_marker_.Style()->Visibility() != EVisibility::kVisible) {
    BlockPainter(layout_details_marker_).Paint(paint_info, paint_offset);
    return;
  }

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_details_marker_, paint_info.phase))
    return;

  LayoutPoint box_origin(paint_offset + layout_details_marker_.Location());
  LayoutRect overflow_rect(layout_details_marker_.VisualOverflowRect());
  overflow_rect.MoveBy(box_origin);

  if (!paint_info.GetCullRect().IntersectsCullRect(overflow_rect))
    return;

  DrawingRecorder recorder(paint_info.context, layout_details_marker_,
                           paint_info.phase, overflow_rect);
  const Color color(layout_details_marker_.ResolveColor(CSSPropertyColor));
  paint_info.context.SetFillColor(color);

  box_origin.Move(
      layout_details_marker_.BorderLeft() +
          layout_details_marker_.PaddingLeft(),
      layout_details_marker_.BorderTop() + layout_details_marker_.PaddingTop());
  paint_info.context.FillPath(GetPath(box_origin));
}

static Path CreatePath(const FloatPoint* path) {
  Path result;
  result.MoveTo(FloatPoint(path[0].X(), path[0].Y()));
  for (int i = 1; i < 4; ++i)
    result.AddLineTo(FloatPoint(path[i].X(), path[i].Y()));
  return result;
}

static Path CreateDownArrowPath() {
  FloatPoint points[4] = {FloatPoint(0.0f, 0.07f), FloatPoint(0.5f, 0.93f),
                          FloatPoint(1.0f, 0.07f), FloatPoint(0.0f, 0.07f)};
  return CreatePath(points);
}

static Path CreateUpArrowPath() {
  FloatPoint points[4] = {FloatPoint(0.0f, 0.93f), FloatPoint(0.5f, 0.07f),
                          FloatPoint(1.0f, 0.93f), FloatPoint(0.0f, 0.93f)};
  return CreatePath(points);
}

static Path CreateLeftArrowPath() {
  FloatPoint points[4] = {FloatPoint(1.0f, 0.0f), FloatPoint(0.14f, 0.5f),
                          FloatPoint(1.0f, 1.0f), FloatPoint(1.0f, 0.0f)};
  return CreatePath(points);
}

static Path CreateRightArrowPath() {
  FloatPoint points[4] = {FloatPoint(0.0f, 0.0f), FloatPoint(0.86f, 0.5f),
                          FloatPoint(0.0f, 1.0f), FloatPoint(0.0f, 0.0f)};
  return CreatePath(points);
}

Path DetailsMarkerPainter::GetCanonicalPath() const {
  switch (layout_details_marker_.GetOrientation()) {
    case LayoutDetailsMarker::kLeft:
      return CreateLeftArrowPath();
    case LayoutDetailsMarker::kRight:
      return CreateRightArrowPath();
    case LayoutDetailsMarker::kUp:
      return CreateUpArrowPath();
    case LayoutDetailsMarker::kDown:
      return CreateDownArrowPath();
  }

  return Path();
}

Path DetailsMarkerPainter::GetPath(const LayoutPoint& origin) const {
  Path result = GetCanonicalPath();
  result.Transform(AffineTransform().Scale(
      layout_details_marker_.ContentWidth().ToFloat(),
      layout_details_marker_.ContentHeight().ToFloat()));
  result.Translate(FloatSize(origin.X().ToFloat(), origin.Y().ToFloat()));
  return result;
}

}  // namespace blink
