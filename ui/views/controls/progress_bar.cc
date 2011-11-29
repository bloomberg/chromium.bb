// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/insets.h"
#include "views/background.h"
#include "views/border.h"
#include "views/painter.h"

namespace {

// Corner radius for the progress bar's border.
const int kCornerRadius = 3;

// Progress bar's border width
const int kBorderWidth = 1;

void AddRoundRectPathWithPadding(int x, int y,
                                 int w, int h,
                                 int corner_radius,
                                 SkScalar padding,
                                 SkPath* path) {
  DCHECK(path);
  if (path == NULL)
    return;
  SkRect rect;
  rect.set(
      SkIntToScalar(x) + padding, SkIntToScalar(y) + padding,
      SkIntToScalar(x + w) - padding, SkIntToScalar(y + h) - padding);
  path->addRoundRect(
      rect,
      SkIntToScalar(corner_radius) - padding,
      SkIntToScalar(corner_radius) - padding);
}

void AddRoundRectPath(int x, int y,
                      int w, int h,
                      int corner_radius,
                      SkPath* path) {
  static const SkScalar half = SkIntToScalar(1) / 2;
  AddRoundRectPathWithPadding(x, y, w, h, corner_radius, half, path);
}

void FillRoundRect(gfx::Canvas* canvas,
                   int x, int y,
                   int w, int h,
                   int corner_radius,
                   const SkColor colors[],
                   const SkScalar points[],
                   int count,
                   bool gradient_horizontal) {
  SkPath path;
  AddRoundRectPath(x, y, w, h, corner_radius, &path);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);

  SkPoint p[2];
  p[0].set(SkIntToScalar(x), SkIntToScalar(y));
  if (gradient_horizontal) {
    p[1].set(SkIntToScalar(x + w), SkIntToScalar(y));
  } else {
    p[1].set(SkIntToScalar(x), SkIntToScalar(y + h));
  }
  SkShader* s = SkGradientShader::CreateLinear(
      p, colors, points, count, SkShader::kClamp_TileMode, NULL);
  paint.setShader(s);
  // Need to unref shader, otherwise never deleted.
  s->unref();

  canvas->GetSkCanvas()->drawPath(path, paint);
}

void FillRoundRect(gfx::Canvas* canvas,
                   int x, int y,
                   int w, int h,
                   int corner_radius,
                   SkColor gradient_start_color,
                   SkColor gradient_end_color,
                   bool gradient_horizontal) {
  if (gradient_start_color != gradient_end_color) {
    SkColor colors[2] = { gradient_start_color, gradient_end_color };
    FillRoundRect(canvas, x, y, w, h, corner_radius,
                  colors, NULL, 2, gradient_horizontal);
  } else {
    SkPath path;
    AddRoundRectPath(x, y, w, h, corner_radius, &path);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(gradient_start_color);
    canvas->GetSkCanvas()->drawPath(path, paint);
  }
}

void StrokeRoundRect(gfx::Canvas* canvas,
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
  canvas->GetSkCanvas()->drawPath(path, paint);
}

}  // namespace

namespace views {

// static
const char ProgressBar::kViewClassName[] = "views/ProgressBar";

ProgressBar::ProgressBar()
    : min_display_value_(0.0),
      max_display_value_(1.0),
      current_value_(0.0) {
}

ProgressBar::~ProgressBar() {
}

void ProgressBar::SetDisplayRange(double min_display_value,
                                  double max_display_value) {
  if (min_display_value != min_display_value_ ||
      max_display_value != max_display_value_) {
    DCHECK(min_display_value < max_display_value);
    min_display_value_ = min_display_value;
    max_display_value_ = max_display_value;
    SchedulePaint();
  }
}

void ProgressBar::SetValue(double value) {
  if (value != current_value_) {
    current_value_ = value;
    SchedulePaint();
  }
}

void ProgressBar::SetTooltipText(const string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool ProgressBar::GetTooltipText(const gfx::Point& p, string16* tooltip) const {
  DCHECK(tooltip);
  if (tooltip == NULL)
    return false;
  tooltip->assign(tooltip_text_);
  return !tooltip_text_.empty();
}

void ProgressBar::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PROGRESSBAR;
  state->state = ui::AccessibilityTypes::STATE_READONLY;
}

gfx::Size ProgressBar::GetPreferredSize() {
  return gfx::Size(100, 16);
}

std::string ProgressBar::GetClassName() const {
  return kViewClassName;
}

void ProgressBar::OnPaint(gfx::Canvas* canvas) {
  const double capped_value = std::min(
      std::max(current_value_, min_display_value_), max_display_value_);
  const double capped_fraction =
      (capped_value - min_display_value_) /
      (max_display_value_ - min_display_value_);
  const int progress_width = static_cast<int>(width() * capped_fraction + 0.5);

#if defined(OS_CHROMEOS)
  const SkColor background_colors[] = {
    SkColorSetRGB(0xBB, 0xBB, 0xBB),
    SkColorSetRGB(0xE7, 0xE7, 0xE7),
    SkColorSetRGB(0xFE, 0xFE, 0xFE)
  };

  const SkScalar background_points[] = {
    SkDoubleToScalar(0),
    SkDoubleToScalar(0.1),
    SkDoubleToScalar(1)
  };
  const SkColor background_border_color = SkColorSetRGB(0xA1, 0xA1, 0xA1);

  // Draw background.
  FillRoundRect(canvas,
                0, 0, width(), height(),
                kCornerRadius,
                background_colors,
                background_points,
                arraysize(background_colors),
                false);
  StrokeRoundRect(canvas,
                  0, 0,
                  width(), height(),
                  kCornerRadius,
                  background_border_color,
                  kBorderWidth);

  if (progress_width > 1) {
    const bool enabled = IsEnabled();

    const SkColor bar_color_start = enabled ?
        SkColorSetRGB(100, 116, 147) :
        SkColorSetRGB(229, 232, 237);
    const SkColor bar_color_end = enabled ?
        SkColorSetRGB(65, 73, 87) :
        SkColorSetRGB(224, 225, 227);

    const SkColor bar_outer_color = enabled ?
        SkColorSetRGB(0x4A, 0x4A, 0x4A) :
        SkColorSetARGB(0x80, 0x4A, 0x4A, 0x4A);

    const SkColor bar_inner_border_color =
        SkColorSetARGB(0x3F, 0xFF, 0xFF, 0xFF);  // 0.25 white
    const SkColor bar_inner_shadow_color =
        SkColorSetARGB(0x54, 0xFF, 0xFF, 0xFF);  // 0.33 white

    // Draw bar background
    FillRoundRect(canvas,
                  0, 0, progress_width, height(),
                  kCornerRadius,
                  bar_color_start,
                  bar_color_end,
                  false);

    // Draw inner stroke and shadow if wide enough.
    if (progress_width > 2 * kBorderWidth) {
      canvas->GetSkCanvas()->save();

      SkPath inner_path;
      AddRoundRectPathWithPadding(
          0, 0, progress_width, height(),
          kCornerRadius,
          SkIntToScalar(kBorderWidth),
          &inner_path);
      canvas->GetSkCanvas()->clipPath(inner_path);

      // Draw bar inner stroke
      StrokeRoundRect(canvas,
                      kBorderWidth, kBorderWidth,
                      progress_width - 2 * kBorderWidth,
                      height() - 2 * kBorderWidth,
                      kCornerRadius - kBorderWidth,
                      bar_inner_border_color,
                      kBorderWidth);

      // Draw bar inner shadow
      StrokeRoundRect(canvas,
                      0, kBorderWidth, progress_width, height(),
                      kCornerRadius,
                      bar_inner_shadow_color,
                      kBorderWidth);

      canvas->GetSkCanvas()->restore();
    }

    // Draw bar stroke
    StrokeRoundRect(canvas,
                    0, 0, progress_width, height(),
                    kCornerRadius,
                    bar_outer_color,
                    kBorderWidth);
  }
#else
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
  if (progress_width > 1) {
    FillRoundRect(canvas,
                  0, 0,
                  progress_width, height(),
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
#endif
}

}  // namespace views
