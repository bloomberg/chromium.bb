// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/throbber.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/resources/grit/views_resources.h"

namespace views {

Throbber::Throbber() : checked_(false), checkmark_(nullptr) {
}

Throbber::~Throbber() {
  Stop();
}

void Throbber::Start() {
  if (IsRunning())
    return;

  start_time_ = base::TimeTicks::Now();
  const int kFrameTimeMs = 30;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kFrameTimeMs), this,
               &Throbber::SchedulePaint);
  SchedulePaint();  // paint right away
}

void Throbber::Stop() {
  if (!IsRunning())
    return;

  timer_.Stop();
  SchedulePaint();
}

void Throbber::SetChecked(bool checked) {
  if (checked == checked_)
    return;

  checked_ = checked;
  SchedulePaint();
}

gfx::Size Throbber::GetPreferredSize() const {
  const int kDefaultDiameter = 16;
  return gfx::Size(kDefaultDiameter, kDefaultDiameter);
}

void Throbber::OnPaint(gfx::Canvas* canvas) {
  if (!IsRunning()) {
    if (checked_) {
      if (!checkmark_) {
        checkmark_ = ui::ResourceBundle::GetSharedInstance()
                         .GetImageNamed(IDR_CHECKMARK)
                         .ToImageSkia();
      }

      int checkmark_x = (width() - checkmark_->width()) / 2;
      int checkmark_y = (height() - checkmark_->height()) / 2;
      canvas->DrawImageInt(*checkmark_, checkmark_x, checkmark_y);
    }
    return;
  }

  SkColor color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_ThrobberSpinningColor);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  gfx::PaintThrobberSpinning(canvas, GetContentsBounds(), color, elapsed_time);
}

bool Throbber::IsRunning() const {
  return timer_.IsRunning();
}

// Smoothed throbber ---------------------------------------------------------

// Delay after work starts before starting throbber, in milliseconds.
static const int kStartDelay = 200;

// Delay after work stops before stopping, in milliseconds.
static const int kStopDelay = 50;

SmoothedThrobber::SmoothedThrobber()
    : start_delay_ms_(kStartDelay), stop_delay_ms_(kStopDelay) {
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

}  // namespace views
