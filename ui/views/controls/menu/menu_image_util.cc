// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_image_util.h"

#include "base/i18n/rtl.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/point.h"

namespace {

// Size of the radio button inciator.
const int kSelectedIndicatorSize = 5;
const int kIndicatorSize = 10;

// Used for the radio indicator. See theme_draw for details.
const double kGradientStop = .5;
const SkColor kGradient0 = SkColorSetRGB(255, 255, 255);
const SkColor kGradient1 = SkColorSetRGB(255, 255, 255);
const SkColor kGradient2 = SkColorSetRGB(0xD8, 0xD8, 0xD8);
const SkColor kBaseStroke = SkColorSetRGB(0x8F, 0x8F, 0x8F);
const SkColor kRadioButtonIndicatorGradient0 = SkColorSetRGB(0, 0, 0);
const SkColor kRadioButtonIndicatorGradient1 = SkColorSetRGB(0x83, 0x83, 0x83);
const SkColor kIndicatorStroke = SkColorSetRGB(0, 0, 0);

SkBitmap* CreateRadioButtonImage(bool selected) {
  // + 2 (1px on each side) to cover rounding error.
  gfx::CanvasSkia canvas(kIndicatorSize + 2, kIndicatorSize + 2, false);
  canvas.Translate(gfx::Point(1, 1));

  SkPoint gradient_points[3];
  gradient_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
  gradient_points[1].set(
      SkIntToScalar(0),
      SkIntToScalar(static_cast<int>(kIndicatorSize * kGradientStop)));
  gradient_points[2].set(SkIntToScalar(0), SkIntToScalar(kIndicatorSize));
  SkColor gradient_colors[3] = { kGradient0, kGradient1, kGradient2 };
  SkShader* shader = SkGradientShader::CreateLinear(
      gradient_points, gradient_colors, NULL, arraysize(gradient_points),
      SkShader::kClamp_TileMode, NULL);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setShader(shader);
  shader->unref();
  int radius = kIndicatorSize / 2;
  canvas.sk_canvas()->drawCircle(radius, radius, radius, paint);

  paint.setStrokeWidth(SkIntToScalar(0));
  paint.setShader(NULL);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setColor(kBaseStroke);
  canvas.sk_canvas()->drawCircle(radius, radius, radius, paint);

  if (selected) {
    SkPoint selected_gradient_points[2];
    selected_gradient_points[0].set(SkIntToScalar(0), SkIntToScalar(0));
    selected_gradient_points[1].set(
        SkIntToScalar(0),
        SkIntToScalar(kSelectedIndicatorSize));
    SkColor selected_gradient_colors[2] = { kRadioButtonIndicatorGradient0,
                                            kRadioButtonIndicatorGradient1 };
    shader = SkGradientShader::CreateLinear(
        selected_gradient_points, selected_gradient_colors, NULL,
        arraysize(selected_gradient_points), SkShader::kClamp_TileMode, NULL);
    paint.setShader(shader);
    shader->unref();
    paint.setStyle(SkPaint::kFill_Style);
    canvas.sk_canvas()->drawCircle(radius, radius,
                                   kSelectedIndicatorSize / 2, paint);

    paint.setStrokeWidth(SkIntToScalar(0));
    paint.setShader(NULL);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kIndicatorStroke);
    canvas.sk_canvas()->drawCircle(radius, radius,
                                   kSelectedIndicatorSize / 2, paint);
  }
  return new SkBitmap(canvas.ExtractBitmap());
}

SkBitmap* GetRtlSubmenuArrowImage() {
  static SkBitmap* kRtlArrow = NULL;
  if (!kRtlArrow) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SkBitmap* r = rb.GetBitmapNamed(IDR_MENU_ARROW);
    gfx::CanvasSkia canvas(r->width(), r->height(), false);
    canvas.Scale(-1, 1);
    canvas.DrawBitmapInt(*r, - r->width(), 0);
    kRtlArrow = new SkBitmap(canvas.ExtractBitmap());
  }
  return kRtlArrow;
}

}  // namespace

namespace views {

const SkBitmap* GetRadioButtonImage(bool selected) {
  static const SkBitmap* kRadioOn = CreateRadioButtonImage(true);
  static const SkBitmap* kRadioOff = CreateRadioButtonImage(false);

  return selected ? kRadioOn : kRadioOff;
}

const SkBitmap* GetSubmenuArrowImage() {
  return base::i18n::IsRTL() ?
      GetRtlSubmenuArrowImage() :
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_MENU_ARROW);
}

}  // namespace views;
