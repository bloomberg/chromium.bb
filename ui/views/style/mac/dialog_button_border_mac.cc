// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/mac/dialog_button_border_mac.h"

#include "base/logging.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {
namespace {

// Type to map button states to a corresponding SkColor.
typedef const SkColor ColorByState[Button::STATE_COUNT];

// If a state is added, ColorByState will silently fill with zeros, so assert.
static_assert(Button::STATE_COUNT == 4,
              "DialogButtonBorderMac assumes 4 button states.");

// Corner radius of rounded rectangles.
const SkScalar kCornerRadius = 2;

// Vertical offset of the drop shadow and the inner highlight shadow.
const SkScalar kShadowOffsetY = 1;

// Shadow blur radius of the inner highlight shadow.
const double kInnerShadowBlurRadius = 2.0;

// Default border insets, to provide text padding.
const int kPaddingX = 14;
const int kPaddingY = 4;

skia::RefPtr<SkShader> GetButtonGradient(int height,
                                         Button::ButtonState state) {
  ColorByState start = {0xFFF0F0F0, 0xFFF4F4F4, 0xFFEBEBEB, 0xFFEDEDED};
  ColorByState end = {0xFFE0E0E0, 0xFFE4E4E4, 0xFFDBDBDB, 0xFFDEDEDE};

  SkPoint gradient_points[2];
  gradient_points[0].iset(0, 0);
  gradient_points[1].iset(0, height);

  SkColor gradient_colors[] = {start[state], start[state], end[state]};
  SkScalar gradient_positions[] = {0.0, 0.38, 1.0};

  skia::RefPtr<SkShader> gradient_shader =
      skia::AdoptRef(SkGradientShader::CreateLinear(
          gradient_points, gradient_colors, gradient_positions, 3,
          SkShader::kClamp_TileMode));
  return gradient_shader;
}

void DrawConstrainedButtonBackground(const SkRect& button_rect,
                                     SkCanvas* canvas,
                                     Button::ButtonState button_state) {
  // Extend the size of the SkRect so the border stroke is drawn over it on all
  // sides.
  SkRect rect(button_rect);
  rect.fRight += 1;
  rect.fBottom += 1;

  SkPaint paint;

  // Drop Shadow.
  ColorByState shadow = {0x14000000, 0x1F000000, 0x00000000, 0x00000000};
  const double blur = 0.0;
  std::vector<gfx::ShadowValue> shadows(
      1, gfx::ShadowValue(gfx::Vector2d(0, kShadowOffsetY), blur,
                          shadow[button_state]));
  skia::RefPtr<SkDrawLooper> looper = gfx::CreateShadowDrawLooper(shadows);
  paint.setLooper(looper.get());

  // Background.
  skia::RefPtr<SkShader> gradient_shader =
      GetButtonGradient(rect.height(), button_state);
  paint.setShader(gradient_shader.get());
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  canvas->drawRoundRect(rect, kCornerRadius, kCornerRadius, paint);
}

// Draws an inner box shadow inside a rounded rectangle of size |rect|. The
// technique: draw a black "ring" around the outside of the button cell. Then
// clip out everything except the shadow it casts. Works similar to Blink's
// GraphicsContext::drawInnerShadow().
void DrawRoundRectInnerShadow(const SkRect& rect,
                              SkCanvas* canvas,
                              SkColor shadow_color) {
  const gfx::Vector2d shadow_offset(0, kShadowOffsetY);
  SkRect outer(rect);
  outer.outset(abs(shadow_offset.x()) + kInnerShadowBlurRadius,
               abs(shadow_offset.y()) + kInnerShadowBlurRadius);

  SkPath path;
  path.addRect(outer);
  path.setFillType(SkPath::kEvenOdd_FillType);
  path.addRoundRect(rect, kCornerRadius, kCornerRadius);  // Poke a hole.

  // Inset the clip to cater for the border stroke.
  SkPath clip;
  clip.addRoundRect(rect.makeInset(0.5, 0.5), kCornerRadius, kCornerRadius);

  SkPaint paint;
  std::vector<gfx::ShadowValue> shadows(
      1, gfx::ShadowValue(shadow_offset, kInnerShadowBlurRadius, shadow_color));
  skia::RefPtr<SkDrawLooper> looper = gfx::CreateShadowDrawLooper(shadows);
  paint.setLooper(looper.get());
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(SK_ColorBLACK);  // Note: Entirely clipped.

  canvas->save();
  canvas->clipPath(clip, SkRegion::kIntersect_Op, true /* antialias */);
  canvas->drawPath(path, paint);
  canvas->restore();
}

}  // namespace

DialogButtonBorderMac::DialogButtonBorderMac() {
  set_insets(gfx::Insets(kPaddingY, kPaddingX, kPaddingY, kPaddingX));
}

DialogButtonBorderMac::~DialogButtonBorderMac() {}

void DialogButtonBorderMac::Paint(const View& view, gfx::Canvas* canvas) {
  DCHECK(CustomButton::AsCustomButton(&view));
  const CustomButton& button = static_cast<const CustomButton&>(view);
  SkCanvas* canvas_skia = canvas->sk_canvas();

  // Inset all sides for the rounded rectangle stroke. Inset again to make room
  // for the shadows (while keeping the text centered).
  SkRect sk_rect = gfx::RectToSkRect(view.GetLocalBounds());
  sk_rect.inset(2.0, 2.0);

  DrawConstrainedButtonBackground(sk_rect, canvas_skia, button.state());

  // Offset so that strokes are contained within the pixel boundary.
  sk_rect.offset(0.5, 0.5);
  ColorByState highlight = {0xBFFFFFFF, 0xF2FFFFFF, 0x24000000, 0x00000000};
  DrawRoundRectInnerShadow(sk_rect, canvas_skia, highlight[button.state()]);

  // Border or focus ring.
  SkPaint paint;
  ColorByState border = {0x40000000, 0x4D000000, 0x4D000000, 0x1F000000};
  const SkColor focus_border = {0xFF5DA5FF};
  paint.setStrokeWidth(1);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  if (button.HasFocus() && button.state() != Button::STATE_PRESSED)
    paint.setColor(focus_border);
  else
    paint.setColor(border[button.state()]);
  canvas_skia->drawRoundRect(sk_rect, kCornerRadius, kCornerRadius, paint);
}

gfx::Size DialogButtonBorderMac::GetMinimumSize() const {
  return gfx::Size(100, 30);
}

}  // namespace views
