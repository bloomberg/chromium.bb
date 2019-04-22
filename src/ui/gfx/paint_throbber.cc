// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/paint_throbber.h"

#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

// The maximum size of the "spinning" state arc, in degrees.
const int64_t kMaxArcSize = 270;

// The amount of time it takes to grow the "spinning" arc from 0 to 270 degrees.
const int64_t kArcTimeMs = 666;

// The amount of time it takes for the "spinning" throbber to make a full
// rotation.
const int64_t kRotationTimeMs = 1568;

void PaintArc(Canvas* canvas,
              const Rect& bounds,
              SkColor color,
              SkScalar start_angle,
              SkScalar sweep,
              base::Optional<SkScalar> stroke_width) {
  if (!stroke_width) {
    // Stroke width depends on size.
    // . For size < 28:          3 - (28 - size) / 16
    // . For 28 <= size:         (8 + size) / 12
    stroke_width = bounds.width() < 28
                       ? 3.0 - SkIntToScalar(28 - bounds.width()) / 16.0
                       : SkIntToScalar(bounds.width() + 8) / 12.0;
  }
  Rect oval = bounds;
  // Inset by half the stroke width to make sure the whole arc is inside
  // the visible rect.
  int inset = SkScalarCeilToInt(*stroke_width / 2.0);
  oval.Inset(inset, inset);

  SkPath path;
  path.arcTo(RectToSkRect(oval), start_angle, sweep, true);

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setStrokeWidth(*stroke_width);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  canvas->DrawPath(path, flags);
}

void CalculateWaitingAngles(const base::TimeDelta& elapsed_time,
                            int64_t* start_angle,
                            int64_t* sweep) {
  // Calculate start and end points. The angles are counter-clockwise because
  // the throbber spins counter-clockwise. The finish angle starts at 12 o'clock
  // (90 degrees) and rotates steadily. The start angle trails 180 degrees
  // behind, except for the first half revolution, when it stays at 12 o'clock.
  base::TimeDelta revolution_time = base::TimeDelta::FromMilliseconds(1320);
  int64_t twelve_oclock = 90;
  int64_t finish_angle_cc =
      twelve_oclock + 360 * elapsed_time / revolution_time;
  int64_t start_angle_cc = std::max(finish_angle_cc - 180, twelve_oclock);

  // Negate the angles to convert to the clockwise numbers Skia expects.
  if (start_angle)
    *start_angle = -finish_angle_cc;
  if (sweep)
    *sweep = finish_angle_cc - start_angle_cc;
}

// This is a Skia port of the MD spinner SVG. The |start_angle| rotation
// here corresponds to the 'rotate' animation.
void PaintThrobberSpinningWithStartAngle(
    Canvas* canvas,
    const Rect& bounds,
    SkColor color,
    const base::TimeDelta& elapsed_time,
    int64_t start_angle,
    base::Optional<SkScalar> stroke_width) {
  // The sweep angle ranges from -270 to 270 over 1333ms. CSS
  // animation timing functions apply in between key frames, so we have to
  // break up the 1333ms into two keyframes (-270 to 0, then 0 to 270).
  base::TimeDelta arc_time = base::TimeDelta::FromMilliseconds(kArcTimeMs);
  double arc_size_progress = static_cast<double>(elapsed_time.InMicroseconds() %
                                                 arc_time.InMicroseconds()) /
                             arc_time.InMicroseconds();
  // This tween is equivalent to cubic-bezier(0.4, 0.0, 0.2, 1).
  double sweep = kMaxArcSize * Tween::CalculateValue(Tween::FAST_OUT_SLOW_IN,
                                                     arc_size_progress);
  int64_t sweep_keyframe = (elapsed_time / arc_time) % 2;
  if (sweep_keyframe == 0)
    sweep -= kMaxArcSize;

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
  PaintArc(canvas, bounds, color, start_angle + rot_keyframe * kMaxArcSize,
           sweep, stroke_width);
}

}  // namespace

void PaintThrobberSpinning(Canvas* canvas,
                           const Rect& bounds,
                           SkColor color,
                           const base::TimeDelta& elapsed_time,
                           base::Optional<SkScalar> stroke_width) {
  base::TimeDelta rotation_time =
      base::TimeDelta::FromMilliseconds(kRotationTimeMs);
  int64_t start_angle = 270 + 360 * elapsed_time / rotation_time;
  PaintThrobberSpinningWithStartAngle(canvas, bounds, color, elapsed_time,
                                      start_angle, stroke_width);
}

void PaintThrobberWaiting(Canvas* canvas,
                          const Rect& bounds,
                          SkColor color,
                          const base::TimeDelta& elapsed_time,
                          base::Optional<SkScalar> stroke_width) {
  int64_t start_angle = 0, sweep = 0;
  CalculateWaitingAngles(elapsed_time, &start_angle, &sweep);
  PaintArc(canvas, bounds, color, start_angle, sweep, stroke_width);
}

void PaintThrobberSpinningAfterWaiting(Canvas* canvas,
                                       const Rect& bounds,
                                       SkColor color,
                                       const base::TimeDelta& elapsed_time,
                                       ThrobberWaitingState* waiting_state,
                                       base::Optional<SkScalar> stroke_width) {
  int64_t waiting_start_angle = 0, waiting_sweep = 0;
  CalculateWaitingAngles(waiting_state->elapsed_time, &waiting_start_angle,
                         &waiting_sweep);

  // |arc_time_offset| is the effective amount of time one would have to wait
  // for the "spinning" sweep to match |waiting_sweep|. Brute force calculation.
  if (waiting_state->arc_time_offset.is_zero()) {
    for (int64_t arc_time_it = 0; arc_time_it <= kArcTimeMs; ++arc_time_it) {
      double arc_size_progress = static_cast<double>(arc_time_it) / kArcTimeMs;
      if (kMaxArcSize * Tween::CalculateValue(Tween::FAST_OUT_SLOW_IN,
                                              arc_size_progress) >=
          waiting_sweep) {
        // Add kArcTimeMs to sidestep the |sweep_keyframe == 0| offset below.
        waiting_state->arc_time_offset =
            base::TimeDelta::FromMilliseconds(arc_time_it + kArcTimeMs);
        break;
      }
    }
  }

  // Blend the color between "waiting" and "spinning" states.
  base::TimeDelta color_fade_time = base::TimeDelta::FromMilliseconds(900);
  float color_progress = 1.0f;
  if (elapsed_time < color_fade_time) {
    color_progress = static_cast<float>(Tween::CalculateValue(
        Tween::LINEAR_OUT_SLOW_IN,
        static_cast<double>(elapsed_time.InMicroseconds()) /
            color_fade_time.InMicroseconds()));
  }
  SkColor blend_color =
      color_utils::AlphaBlend(color, waiting_state->color, color_progress);

  int64_t start_angle =
      waiting_start_angle +
      360 * elapsed_time / base::TimeDelta::FromMilliseconds(kRotationTimeMs);
  base::TimeDelta effective_elapsed_time =
      elapsed_time + waiting_state->arc_time_offset;

  PaintThrobberSpinningWithStartAngle(canvas, bounds, blend_color,
                                      effective_elapsed_time, start_angle,
                                      stroke_width);
}

GFX_EXPORT void PaintNewThrobberWaiting(Canvas* canvas,
                                        const RectF& throbber_container_bounds,
                                        SkColor color,
                                        const base::TimeDelta& elapsed_time) {
  // The throbber bounces back and forth. We map the elapsed time to 0->2. Time
  // 0->1 represents when the throbber moves left to right, time 1->2 represents
  // right to left.
  float time =
      2.0f *
      (elapsed_time.InMilliseconds() % kNewThrobberWaitingAnimationCycleMs) /
      kNewThrobberWaitingAnimationCycleMs;
  // 1 -> 2 values mirror back to 1 -> 0 values to represent right-to-left.
  const bool going_back = time > 1.0f;
  if (going_back)
    time = 2.0f - time;
  // This animation should be fast in the middle and slow at the edges.
  time = Tween::CalculateValue(Tween::EASE_IN_OUT, time);
  const float min_width = throbber_container_bounds.height();
  // The throbber animation stretches longer when moving in (left to right) than
  // when going back.
  const float throbber_width =
      (going_back ? 0.75f : 1.0f) * throbber_container_bounds.width();

  // These bounds keep at least |min_width| of the throbber visible (inside the
  // throbber bounds).
  const float min_x =
      throbber_container_bounds.x() - throbber_width + min_width;
  const float max_x = throbber_container_bounds.right() - min_width;

  RectF bounds = throbber_container_bounds;
  // Linear interpolation between |min_x| and |max_x|.
  bounds.set_x(time * (max_x - min_x) + min_x);
  bounds.set_width(throbber_width);
  // The throbber is designed to go out of bounds, but it should not be rendered
  // outside |throbber_container_bounds|. This clips the throbber to the edges,
  // which gives a smooth bouncing effect.
  bounds.Intersect(throbber_container_bounds);

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  // Draw with circular end caps.
  canvas->DrawRoundRect(bounds, bounds.height() / 2, flags);
}

}  // namespace gfx
