// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_throbber.h"

#include "base/time/time.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

void PaintArc(Canvas* canvas,
              const Rect& bounds,
              SkColor color,
              SkScalar start_angle,
              SkScalar sweep) {
  // Stroke width depends on size.
  // . For size < 28:          3 - (28 - size) / 16
  // . For 28 <= size:         (8 + size) / 12
  SkScalar stroke_width = bounds.width() < 28
                              ? 3.0 - SkIntToScalar(28 - bounds.width()) / 16.0
                              : SkIntToScalar(bounds.width() + 8) / 12.0;
  Rect oval = bounds;
  // Inset by half the stroke width to make sure the whole arc is inside
  // the visible rect.
  int inset = SkScalarCeilToInt(stroke_width / 2.0);
  oval.Inset(inset, inset);

  SkPath path;
  path.arcTo(RectToSkRect(oval), start_angle, sweep, true);

  SkPaint paint;
  paint.setColor(color);
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeWidth(stroke_width);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setAntiAlias(true);
  canvas->DrawPath(path, paint);
}

}  // namespace

void PaintThrobberSpinning(Canvas* canvas,
    const Rect& bounds, SkColor color, const base::TimeDelta& elapsed_time) {
  // This is a Skia port of the MD spinner SVG. The |start_angle| rotation
  // here corresponds to the 'rotate' animation.
  base::TimeDelta rotation_time = base::TimeDelta::FromMilliseconds(1568);
  int64_t start_angle = 270 + 360 * elapsed_time / rotation_time;

  // The sweep angle ranges from -|arc_size| to |arc_size| over 1333ms. CSS
  // animation timing functions apply in between key frames, so we have to
  // break up the |arc_time| into two keyframes (-arc_size to 0, then 0 to
  // arc_size).
  int64_t arc_size = 270;
  base::TimeDelta arc_time = base::TimeDelta::FromMilliseconds(666);
  double arc_size_progress = static_cast<double>(elapsed_time.InMicroseconds() %
                                                 arc_time.InMicroseconds()) /
                             arc_time.InMicroseconds();
  // This tween is equivalent to cubic-bezier(0.4, 0.0, 0.2, 1).
  double sweep =
      arc_size * Tween::CalculateValue(Tween::FAST_OUT_SLOW_IN,
                                       arc_size_progress);
  int64_t sweep_keyframe = (elapsed_time / arc_time) % 2;
  if (sweep_keyframe == 0)
    sweep -= arc_size;

  // This part makes sure the sweep is at least 5 degrees long. Roughly
  // equivalent to the "magic constants" in SVG's fillunfill animation.
  const double min_sweep_length = 5.0;
  if (sweep >= 0.0 && sweep < min_sweep_length) {
    start_angle -= (min_sweep_length - sweep);
    sweep = min_sweep_length;
  } else if (sweep <= 0.0 && sweep > -min_sweep_length) {
    start_angle += (-min_sweep_length - sweep);
    sweep = -min_sweep_length;
  }

  // To keep the sweep smooth, we have an additional rotation after each
  // |arc_time| period has elapsed. See SVG's 'rot' animation.
  int64_t rot_keyframe = (elapsed_time / (arc_time * 2)) % 4;
  PaintArc(canvas, bounds, color,
           start_angle + rot_keyframe * arc_size, sweep);
}

void PaintThrobberSpinningForFrame(Canvas* canvas,
    const Rect& bounds, SkColor color, uint32_t frame) {
  const uint32_t frame_duration_ms = 30;
  PaintThrobberSpinning(canvas, bounds, color,
      base::TimeDelta::FromMilliseconds(frame * frame_duration_ms));
}

void PaintThrobberWaiting(Canvas* canvas,
    const Rect& bounds, SkColor color, const base::TimeDelta& elapsed_time) {
  // Calculate start and end points. The angles are counter-clockwise because
  // the throbber spins counter-clockwise. The finish angle starts at 12 o'clock
  // (90 degrees) and rotates steadily. The start angle trails 180 degrees
  // behind, except for the first half revolution, when it stays at 12 o'clock.
  base::TimeDelta revolution_time = base::TimeDelta::FromMilliseconds(1320);
  int64_t twelve_oclock = 90;
  int64_t finish_angle = twelve_oclock + 360 * elapsed_time / revolution_time;
  int64_t start_angle = std::max(finish_angle - 180, twelve_oclock);

  // Negate the angles to convert to the clockwise numbers Skia expects.
  PaintArc(canvas, bounds, color, -start_angle, -(finish_angle - start_angle));
}

void PaintThrobberWaitingForFrame(Canvas* canvas,
    const Rect& bounds, SkColor color, uint32_t frame) {
  const uint32_t frame_duration_ms = 30;
  PaintThrobberWaiting(canvas, bounds, color,
      base::TimeDelta::FromMilliseconds(frame * frame_duration_ms));
}

}  // namespace gfx
