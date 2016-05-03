// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/mac/dialog_button_border_mac.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme_mac.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"

using ui::NativeThemeMac;

namespace views {
namespace {

// Corner radius of rounded rectangles.
const SkScalar kCornerRadius = 2;
const SkScalar kFocusCornerRadius = 4;
const SkScalar kFocusRingThickness = 3;

const SkColor kDefaultBorderColor = SkColorSetARGB(0xF2, 0xBA, 0xBA, 0xBA);
const SkColor kHighlightedBorderColor = SkColorSetARGB(0xFF, 0x52, 0x76, 0xFF);
const SkColor kFocusRingColor = SkColorSetARGB(0x80, 0x3B, 0x9A, 0xFC);

// Default border insets, to provide text padding.
const int kPaddingX = 19;
const int kPaddingY = 7;

void DrawDialogButtonBackground(const SkRect& button_rect,
                                SkCanvas* canvas,
                                const LabelButton& button) {
  // Extend the size of the SkRect so the border stroke is drawn over it on all
  // sides.
  SkRect rect(button_rect);
  rect.fRight += 1;
  rect.fBottom += 1;

  NativeThemeMac::ButtonBackgroundType type =
      NativeThemeMac::ButtonBackgroundType::NORMAL;
  if (!button.enabled() || button.state() == Button::STATE_DISABLED)
    type = NativeThemeMac::ButtonBackgroundType::DISABLED;
  else if (button.state() == Button::STATE_PRESSED)
    type = NativeThemeMac::ButtonBackgroundType::PRESSED;
  else if (DialogButtonBorderMac::ShouldRenderDefault(button))
    type = NativeThemeMac::ButtonBackgroundType::HIGHLIGHTED;

  // Background.
  SkPaint paint;
  paint.setShader(
      NativeThemeMac::GetButtonBackgroundShader(type, rect.height()));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  canvas->drawRoundRect(rect, kCornerRadius, kCornerRadius, paint);
}

}  // namespace

DialogButtonBorderMac::DialogButtonBorderMac() {
  set_insets(gfx::Insets(kPaddingY, kPaddingX, kPaddingY, kPaddingX));
}

DialogButtonBorderMac::~DialogButtonBorderMac() {}

// static
bool DialogButtonBorderMac::ShouldRenderDefault(const LabelButton& button) {
  // TODO(tapted): Check whether the Widget is active, and only return true here
  // if it is. Plumbing this requires default buttons to also observe Widget
  // activations to ensure text and background colors are properly invalidated.
  return button.is_default();
}

void DialogButtonBorderMac::Paint(const View& view, gfx::Canvas* canvas) {
  // Actually, |view| should be a LabelButton as well, but don't rely too much
  // on RTTI.
  DCHECK(CustomButton::AsCustomButton(&view));
  const LabelButton& button = static_cast<const LabelButton&>(view);
  SkCanvas* canvas_skia = canvas->sk_canvas();

  // Inset all sides for the rounded rectangle stroke. Inset again to make room
  // for the shadows and static focus ring (while keeping the text centered).
  SkRect sk_rect = gfx::RectToSkRect(view.GetLocalBounds());
  sk_rect.inset(kFocusRingThickness, kFocusRingThickness);

  DrawDialogButtonBackground(sk_rect, canvas_skia, button);

  // Offset so that strokes are contained within the pixel boundary.
  sk_rect.offset(0.5, 0.5);

  // Border and focus ring.
  SkColor border_color = kDefaultBorderColor;
  if (button.state() == Button::STATE_PRESSED || ShouldRenderDefault(button))
    border_color = kHighlightedBorderColor;

  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStrokeWidth(1);
  paint.setColor(border_color);
  canvas_skia->drawRoundRect(sk_rect, kCornerRadius, kCornerRadius, paint);

  if (button.HasFocus()) {
    paint.setStrokeWidth(kFocusRingThickness);
    paint.setColor(kFocusRingColor);
    sk_rect.inset(-1, -1);
    canvas_skia->drawRoundRect(sk_rect, kFocusCornerRadius, kFocusCornerRadius,
                               paint);
  }
}

gfx::Size DialogButtonBorderMac::GetMinimumSize() const {
  // Overridden by PlatformStyle. Here, just ensure the minimum size is
  // consistent with the padding.
  return gfx::Size(2 * kPaddingX, 2 * kPaddingY);
}

}  // namespace views
