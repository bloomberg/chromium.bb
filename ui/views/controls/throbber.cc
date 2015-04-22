// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include "base/time/time.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"

using base::Time;
using base::TimeDelta;

namespace views {

Throbber::Throbber(int frame_time_ms,
                   bool paint_while_stopped)
    : running_(false),
      paint_while_stopped_(paint_while_stopped),
      frames_(NULL),
      frame_time_(TimeDelta::FromMilliseconds(frame_time_ms)) {
  SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_THROBBER).ToImageSkia());
}

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (running_)
    return;

  start_time_ = Time::Now();

  timer_.Start(FROM_HERE, frame_time_ - TimeDelta::FromMilliseconds(10),
               this, &Throbber::Run);

  running_ = true;

  SchedulePaint();  // paint right away
}

void Throbber::Stop() {
  if (!running_)
    return;

  timer_.Stop();

  running_ = false;
  SchedulePaint();  // Important if we're not painting while stopped
}

void Throbber::SetFrames(const gfx::ImageSkia* frames) {
  frames_ = frames;
  DCHECK(frames_->width() > 0 && frames_->height() > 0);
  DCHECK(frames_->width() % frames_->height() == 0);
  frame_count_ = frames_->width() / frames_->height();
  PreferredSizeChanged();
}

void Throbber::Run() {
  DCHECK(running_);

  SchedulePaint();
}

gfx::Size Throbber::GetPreferredSize() const {
  return gfx::Size(frames_->height(), frames_->height());
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  if (!running_ && !paint_while_stopped_)
    return;

  const TimeDelta elapsed_time = Time::Now() - start_time_;
  const int current_frame =
      static_cast<int>(elapsed_time / frame_time_) % frame_count_;

  int image_size = frames_->height();
  int image_offset = current_frame * image_size;
  canvas->DrawImageInt(*frames_,
                       image_offset, 0, image_size, image_size,
                       0, 0, image_size, image_size,
                       false);
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

  if (!running() && !start_timer_.IsRunning()) {
    start_timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(start_delay_ms_),
                       this, &SmoothedThrobber::StartDelayOver);
  }
}

void SmoothedThrobber::StartDelayOver() {
  Throbber::Start();
}

void SmoothedThrobber::Stop() {
  if (!running())
    start_timer_.Stop();

  stop_timer_.Stop();
  stop_timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(stop_delay_ms_),
                    this, &SmoothedThrobber::StopDelayOver);
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
  if (!running()) {
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

  gfx::Rect bounds = GetContentsBounds();
  // Inset by half the stroke width to make sure the whole arc is inside
  // the visible rect.
  SkScalar stroke_width = SkIntToScalar(bounds.width()) / 10.0;
  gfx::Rect oval = bounds;
  int inset = SkScalarCeilToInt(stroke_width / 2.0);
  oval.Inset(inset, inset);

  // Calculate start and end points. The angles are counter-clockwise because
  // the throbber spins counter-clockwise. The finish angle starts at 12 o'clock
  // (90 degrees) and rotates steadily. The start angle trails 180 degrees
  // behind, except for the first half revolution, when it stays at 12 o'clock.
  base::TimeDelta revolution_time = base::TimeDelta::FromMilliseconds(1320);
  base::TimeDelta elapsed_time = base::Time::Now() - start_time();
  int64_t twelve_oclock = 90;
  int64_t finish_angle = twelve_oclock + 360 * elapsed_time / revolution_time;
  int64_t start_angle = std::max(finish_angle - 180, twelve_oclock);

  SkPath path;
  // Negate the angles to convert to the clockwise numbers Skia expects.
  path.arcTo(gfx::RectToSkRect(oval), -start_angle,
             -(finish_angle - start_angle), true);

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
