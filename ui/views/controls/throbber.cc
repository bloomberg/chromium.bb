// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

namespace views {

Throbber::Throbber(int frame_time_ms, bool paint_while_stopped)
    : paint_while_stopped_(paint_while_stopped),
      frames_(NULL),
      frame_time_(base::TimeDelta::FromMilliseconds(frame_time_ms)) {
  SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_THROBBER).ToImageSkia());
}

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (IsRunning())
    return;

  start_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, frame_time_ - base::TimeDelta::FromMilliseconds(10),
               this, &Throbber::SchedulePaint);
  SchedulePaint();  // paint right away
}

void Throbber::Stop() {
  if (!IsRunning())
    return;

  timer_.Stop();
  SchedulePaint();  // Important if we're not painting while stopped
}

void Throbber::SetFrames(const gfx::ImageSkia* frames) {
  frames_ = frames;
  DCHECK(frames_->width() > 0 && frames_->height() > 0);
  DCHECK(frames_->width() % frames_->height() == 0);
  frame_count_ = frames_->width() / frames_->height();
  PreferredSizeChanged();
}

gfx::Size Throbber::GetPreferredSize() const {
  return gfx::Size(frames_->height(), frames_->height());
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  if (!IsRunning() && !paint_while_stopped_)
    return;

  const base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  const int current_frame =
      static_cast<int>(elapsed_time / frame_time_) % frame_count_;

  int image_size = frames_->height();
  int image_offset = current_frame * image_size;
  canvas->DrawImageInt(*frames_,
                       image_offset, 0, image_size, image_size,
                       0, 0, image_size, image_size,
                       false);
}

bool Throbber::IsRunning() const {
  return timer_.IsRunning();
}

// Smoothed throbber ---------------------------------------------------------

// Delay after work starts before starting throbber, in milliseconds.
static const int kStartDelay = 200;

// Delay after work stops before stopping, in milliseconds.
static const int kStopDelay = 50;

SmoothedThrobber::SmoothedThrobber(int frame_time_ms)
    : Throbber(frame_time_ms, /* paint_while_stopped= */ false),
      start_delay_ms_(kStartDelay),
      stop_delay_ms_(kStopDelay) {
}

SmoothedThrobber::~SmoothedThrobber() {}

void SmoothedThrobber::Start() {
  stop_timer_.Stop();

  if (!IsRunning() && !start_timer_.IsRunning()) {
    start_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(start_delay_ms_), this,
                       &SmoothedThrobber::StartDelayOver);
  }
}

void SmoothedThrobber::StartDelayOver() {
  Throbber::Start();
}

void SmoothedThrobber::Stop() {
  if (!IsRunning())
    start_timer_.Stop();

  stop_timer_.Stop();
  stop_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(stop_delay_ms_), this,
                    &SmoothedThrobber::StopDelayOver);
}

void SmoothedThrobber::StopDelayOver() {
  Throbber::Stop();
}

// Material throbber -----------------------------------------------------------

// The length of a frame in milliseconds.
// TODO(estade): remove the +10 when the -10 is removed from Throbber::Start().
static const int kMaterialThrobberFrameTimeMs = 30 + 10;

MaterialThrobber::MaterialThrobber() :
    Throbber(kMaterialThrobberFrameTimeMs, false),
    preferred_diameter_(0),
    checked_(false),
    checkmark_(nullptr) {
}

MaterialThrobber::~MaterialThrobber() {
}

void MaterialThrobber::SetChecked(bool checked) {
  if (checked == checked_)
    return;

  checked_ = checked;
  SchedulePaint();
}

gfx::Size MaterialThrobber::GetPreferredSize() const {
  if (preferred_diameter_ == 0)
    return Throbber::GetPreferredSize();

  return gfx::Size(preferred_diameter_, preferred_diameter_);
}

int MaterialThrobber::GetHeightForWidth(int w) const {
  return w;
}

void MaterialThrobber::OnPaint(gfx::Canvas* canvas) {
  if (!IsRunning()) {
    if (checked_) {
      if (!checkmark_) {
        checkmark_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_CHECKMARK).ToImageSkia();
      }

      int checkmark_x = (width() - checkmark_->width()) / 2;
      int checkmark_y = (height() - checkmark_->height()) / 2;
      canvas->DrawImageInt(*checkmark_, checkmark_x, checkmark_y);
    }
    return;
  }

  PaintSpinning(canvas);
}

void MaterialThrobber::PaintSpinning(gfx::Canvas* canvas) {
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time();

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
      arc_size * gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
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
  PaintArc(canvas, start_angle + rot_keyframe * arc_size, sweep);
}

void MaterialThrobber::PaintWaiting(gfx::Canvas* canvas) {
  // Calculate start and end points. The angles are counter-clockwise because
  // the throbber spins counter-clockwise. The finish angle starts at 12 o'clock
  // (90 degrees) and rotates steadily. The start angle trails 180 degrees
  // behind, except for the first half revolution, when it stays at 12 o'clock.
  base::TimeDelta revolution_time = base::TimeDelta::FromMilliseconds(1320);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time();
  int64_t twelve_oclock = 90;
  int64_t finish_angle = twelve_oclock + 360 * elapsed_time / revolution_time;
  int64_t start_angle = std::max(finish_angle - 180, twelve_oclock);

  // Negate the angles to convert to the clockwise numbers Skia expects.
  PaintArc(canvas, -start_angle, -(finish_angle - start_angle));
}

void MaterialThrobber::PaintArc(gfx::Canvas* canvas,
                                SkScalar start_angle,
                                SkScalar sweep) {
  gfx::Rect bounds = GetContentsBounds();
  // Inset by half the stroke width to make sure the whole arc is inside
  // the visible rect.
  SkScalar stroke_width = SkIntToScalar(bounds.width()) / 10.0;
  gfx::Rect oval = bounds;
  int inset = SkScalarCeilToInt(stroke_width / 2.0);
  oval.Inset(inset, inset);

  SkPath path;
  path.arcTo(gfx::RectToSkRect(oval), start_angle, sweep, true);

  SkPaint paint;
  // TODO(estade): find a place for this color.
  paint.setColor(SkColorSetRGB(0x42, 0x85, 0xF4));
  paint.setStrokeCap(SkPaint::kRound_Cap);
  paint.setStrokeWidth(stroke_width);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setAntiAlias(true);
  canvas->DrawPath(path, paint);
}

}  // namespace views
