// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/progress_bar.h"

#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
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

static void AddRoundRectPathWithPadding(int x, int y,
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
      SkIntToScalar(corner_radius - padding),
      SkIntToScalar(corner_radius - padding));
}

static void AddRoundRectPath(int x, int y,
                             int w, int h,
                             int corner_radius,
                             SkPath* path) {
  static const SkScalar half = SkIntToScalar(1) / 2;
  AddRoundRectPathWithPadding(x, y, w, h, corner_radius, half, path);
}

static void FillRoundRect(gfx::Canvas* canvas,
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

  canvas->AsCanvasSkia()->drawPath(path, paint);
}

static void FillRoundRect(gfx::Canvas* canvas,
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
    canvas->AsCanvasSkia()->drawPath(path, paint);
  }
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

void ProgressBar::OnPaint(gfx::Canvas* canvas) {
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

  if (progress_ * width() > 1) {
    int progress_width = progress_ * width() / kMaxProgress;

    bool enabled = IsEnabled();

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
      canvas->AsCanvasSkia()->save();

      SkPath inner_path;
      AddRoundRectPathWithPadding(
          0, 0, progress_width, height(),
          kCornerRadius,
          SkIntToScalar(kBorderWidth),
          &inner_path);
      canvas->AsCanvasSkia()->clipPath(inner_path);

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

      canvas->AsCanvasSkia()->restore();
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
#endif
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
  tooltip_text_ = WideToUTF16Hack(tooltip_text);
}

bool ProgressBar::GetTooltipText(const gfx::Point& p, std::wstring* tooltip) {
  DCHECK(tooltip);
  if (tooltip == NULL)
    return false;
  tooltip->assign(UTF16ToWideHack(tooltip_text_));
  return !tooltip_text_.empty();
}

void ProgressBar::SetEnabled(bool enabled) {
  if (enabled == enabled_)
    return;
  View::SetEnabled(enabled);
  // TODO(denisromanov): Need to switch progress bar color here?
}

void ProgressBar::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PROGRESSBAR;
  state->state = ui::AccessibilityTypes::STATE_READONLY;
}

}  // namespace views
