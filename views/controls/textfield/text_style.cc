// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/text_style.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"

namespace {
// Text color for read only.
const SkColor kReadonlyTextColor = SK_ColorDKGRAY;

// Strike line width.
const int kStrikeWidth = 2;
}

namespace views {

TextStyle::TextStyle()
    : foreground_(SK_ColorBLACK),
      strike_(false),
      underline_(false) {
}

TextStyle::~TextStyle() {
}

void TextStyle::DrawString(gfx::Canvas* canvas,
                           string16& text,
                           gfx::Font& base_font,
                           bool readonly,
                           int x, int y, int width, int height) const {
  SkColor text_color = readonly ? kReadonlyTextColor : foreground_;

  gfx::Font font = underline_ ?
      base_font.DeriveFont(0, base_font.GetStyle() | gfx::Font::UNDERLINED) :
      base_font;
  canvas->DrawStringInt(text, font, text_color, x, y, width, height);
  if (strike_) {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(text_color);
    paint.setStrokeWidth(kStrikeWidth);
    canvas->AsCanvasSkia()->drawLine(
        SkIntToScalar(x), SkIntToScalar(y + height),
        SkIntToScalar(x + width), SkIntToScalar(y),
        paint);
  }
}

}  // namespace views
