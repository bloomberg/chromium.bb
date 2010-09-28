// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/progress_bar.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "gfx/canvas_skia.h"
#include "gfx/color_utils.h"
#include "gfx/font.h"
#include "gfx/insets.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "views/background.h"
#include "views/border.h"
#include "views/painter.h"

namespace {

// Corner radius for the progress bar's border.
const int kCornerRadius = 3;

// Progress bar's border width
const int kBorderWidth = 1;

static void AddRoundRectPath(int x, int y,
                             int w, int h,
                             int corner_radius,
                             SkPath* path) {
  DCHECK(path);
  if (path == NULL)
    return;
  SkScalar half = SkIntToScalar(1) / 2;
  SkRect rect;
  rect.set(
      SkIntToScalar(x) + half, SkIntToScalar(y) + half,
      SkIntToScalar(x + w) - half, SkIntToScalar(y + h) - half);
  path->addRoundRect(
      rect,
      SkIntToScalar(corner_radius - half),
      SkIntToScalar(corner_radius - half));
}

static void FillRoundRect(gfx::Canvas* canvas,
                          int x, int y,
                          int w, int h,
                          int corner_radius,
                          SkColor gradient_start_color,
                          SkColor gradient_end_color,
                          bool gradient_horizontal) {
  SkPath path;
  AddRoundRectPath(x, y, w, h, corner_radius, &path);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  if (gradient_start_color != gradient_end_color) {
    SkPoint p[2];
    p[0].set(SkIntToScalar(x), SkIntToScalar(y));
    if (gradient_horizontal) {
      p[1].set(SkIntToScalar(x + w), SkIntToScalar(y));
    } else {
      p[1].set(SkIntToScalar(x), SkIntToScalar(y + h));
    }
    SkColor colors[2] = { gradient_start_color, gradient_end_color };
    SkShader* s = SkGradientShader::CreateLinear(
        p, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();
  } else {
    paint.setColor(gradient_start_color);
  }
  canvas->AsCanvasSkia()->drawPath(path, paint);
}

static void StrokeRoundRect(gfx::Canvas* canvas,
                            int x, int y,
                            int w, int h,
                            int corner_radius,
                            SkColor stroke_color,
                            int stroke_width) {
  SkPath path;
  AddRoundRectPath(x, y, w, h, corner_radius, &path);
  SkPaint paint;
  paint.setShader(NULL);
  paint.setColor(stroke_color);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStrokeWidth(SkIntToScalar(stroke_width));
  canvas->AsCanvasSkia()->drawPath(path, paint);
}

}  // anonymous namespace

namespace views {

// static
const char ProgressBar::kViewClassName[] = "views/ProgressBar";
// static: progress bar's maximum value.
const int ProgressBar::kMaxProgress = 100;


ProgressBar::ProgressBar(): progress_(0) {
}

ProgressBar::~ProgressBar() {
}

gfx::Size ProgressBar::GetPreferredSize() {
  return gfx::Size(100, 16);
}

void ProgressBar::Paint(gfx::Canvas* canvas) {
  SkColor bar_color_start = SkColorSetRGB(81, 138, 223);
  SkColor bar_color_end = SkColorSetRGB(51, 103, 205);
  SkColor background_color_start = SkColorSetRGB(212, 212, 212);
  SkColor background_color_end = SkColorSetRGB(252, 252, 252);
  SkColor border_color = SkColorSetRGB(144, 144, 144);

  FillRoundRect(canvas,
                0, 0, width(), height(),
                kCornerRadius,
                background_color_start,
                background_color_end,
                false);
  if (progress_ * width() > 1) {
    FillRoundRect(canvas,
                  0, 0,
                  progress_ * width() / kMaxProgress, height(),
                  kCornerRadius,
                  bar_color_start,
                  bar_color_end,
                  false);
  }
  StrokeRoundRect(canvas,
                  0, 0,
                  width(), height(),
                  kCornerRadius,
                  border_color,
                  kBorderWidth);
}

std::string ProgressBar::GetClassName() const {
  return kViewClassName;
}

void ProgressBar::SetProgress(int progress) {
  progress_ = progress;
  if (progress_ < 0)
    progress_ = 0;
  else if (progress_ > kMaxProgress)
    progress_ = kMaxProgress;
  SchedulePaint();
}

int ProgressBar::GetProgress() const {
  return progress_;
}

void ProgressBar::AddProgress(int tick) {
  SetProgress(progress_ + tick);
}

void ProgressBar::SetTooltipText(const std::wstring& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool ProgressBar::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  DCHECK(tooltip);
  if (tooltip == NULL)
    return false;
  tooltip->assign(tooltip_text_);
  return !tooltip_text_.empty();
}

void ProgressBar::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;
  View::SetEnabled(enabled);
  // TODO(denisromanov): Need to switch progress bar color here?
}

AccessibilityTypes::Role ProgressBar::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_PROGRESSBAR;
}

AccessibilityTypes::State ProgressBar::GetAccessibleState() {
  return AccessibilityTypes::STATE_READONLY;
}

}  // namespace views
