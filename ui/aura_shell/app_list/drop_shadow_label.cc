// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/app_list/drop_shadow_label.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skbitmap_operations.h"

using views::Label;

namespace aura_shell {

static const int kDefaultDropShadowSize = 2;

DropShadowLabel::DropShadowLabel() : drop_shadow_size_(kDefaultDropShadowSize) {
}

void DropShadowLabel::SetDropShadowSize(int drop_shadow_size) {
  if (drop_shadow_size != drop_shadow_size_) {
    drop_shadow_size_ = drop_shadow_size;
    invalidate_text_size();
    SchedulePaint();
  }
}

void DropShadowLabel::PaintText(gfx::Canvas* canvas,
                                const string16& text,
                                const gfx::Rect& text_bounds,
                                int flags) {
  SkColor text_color = enabled() ? enabled_color() : disabled_color();
  if (drop_shadow_size_ > 0) {
    // To properly render shadow with elliding fade effect, text and shadow
    // is rendered to this canvas first with elliding disable so that underlying
    // code would not mix shadow color into text area because of elliding fade.
    // When that is done and if we need elliding fade, an alpha mask is applied
    // when transfering contents on this canvas to target canvas.
    gfx::Size canvas_size(text_bounds.width() + drop_shadow_size_,
                          text_bounds.height() + drop_shadow_size_);
    gfx::CanvasSkia text_canvas(canvas_size, false);

    const double kShadowOpacity = 0.2;
    const SkColor shadow_color =
        SkColorSetA(SK_ColorBLACK, kShadowOpacity * SkColorGetA(text_color));
    gfx::Size text_size = GetTextSize();
    for (int i = 0; i < drop_shadow_size_; i++) {
      text_canvas.DrawStringInt(text, font(), shadow_color, i, 0,
                                text_size.width(), text_size.height(),
                                flags | gfx::Canvas::NO_ELLIPSIS);
      text_canvas.DrawStringInt(text, font(), shadow_color, i, i,
                                text_size.width(), text_size.height(),
                                flags | gfx::Canvas::NO_ELLIPSIS);
      text_canvas.DrawStringInt(text, font(), shadow_color, 0, i,
                                text_size.width(), text_size.height(),
                                flags | gfx::Canvas::NO_ELLIPSIS);
    }
    text_canvas.DrawStringInt(text, font(), text_color, 0, 0,
                              text_size.width(), text_size.height(),
                              flags | gfx::Canvas::NO_ELLIPSIS);

    const SkBitmap& text_bitmap = const_cast<SkBitmap&>(
        skia::GetTopDevice(*text_canvas.sk_canvas())->accessBitmap(false));

    if (text_size.width() > text_bounds.width() &&
        !(flags & gfx::Canvas::NO_ELLIPSIS)) {
      // Apply an gradient alpha mask for elliding fade effect.
      const double kFadeWidthFactor = 1.5;
      int fade_width = std::min(text_size.width() / 2,
          static_cast<int>(text_size.height() * kFadeWidthFactor));

      const SkColor kColors[] = { SK_ColorWHITE, 0 };
      const SkScalar kPoints[] = { SkIntToScalar(0), SkIntToScalar(1) };
      SkPoint p[2];
      p[0].set(SkIntToScalar(text_bounds.width() - fade_width),
               SkIntToScalar(0));
      p[1].set(SkIntToScalar(text_bounds.width()),
               SkIntToScalar(0));
      SkShader* s = SkGradientShader::CreateLinear(
          p, kColors, kPoints, 2, SkShader::kClamp_TileMode, NULL);

      SkPaint paint;
      paint.setShader(s)->unref();

      gfx::CanvasSkia alpha_canvas(canvas_size, false);
      alpha_canvas.DrawRect(gfx::Rect(canvas_size), paint);

      const SkBitmap& alpha_bitmap = const_cast<SkBitmap&>(
          skia::GetTopDevice(*alpha_canvas.sk_canvas())->accessBitmap(false));
      SkBitmap blended = SkBitmapOperations::CreateMaskedBitmap(text_bitmap,
          alpha_bitmap);
      canvas->DrawBitmapInt(blended, text_bounds.x(), text_bounds.y());
    } else {
      canvas->DrawBitmapInt(text_bitmap, text_bounds.x(), text_bounds.y());
    }
  } else {
    canvas->DrawStringInt(text, font(), text_color, text_bounds.x(),
        text_bounds.y(), text_bounds.width(), text_bounds.height(), flags);
  }

  if (HasFocus() || paint_as_focused()) {
    gfx::Rect focus_bounds = text_bounds;
    focus_bounds.Inset(-Label::kFocusBorderPadding,
                       -Label::kFocusBorderPadding);
    canvas->DrawFocusRect(focus_bounds);
  }
}

gfx::Size DropShadowLabel::GetTextSize() const {
  gfx::Size text_size = Label::GetTextSize();
  text_size.SetSize(text_size.width() + drop_shadow_size_,
                    text_size.height() + drop_shadow_size_);
  return text_size;
}

}  // namespace aura_shell
