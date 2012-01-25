// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas_skia.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/skia_util.h"

namespace {

// Based on |flags| and |text| content, returns whether text should be
// rendered right-to-left.
bool IsTextRTL(int flags, const string16& text) {
  if (flags & gfx::Canvas::FORCE_RTL_DIRECTIONALITY)
    return true;
  if (flags & gfx::Canvas::FORCE_LTR_DIRECTIONALITY)
    return false;
  return base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(text);
}

// Checks each pixel immediately adjacent to the given pixel in the bitmap. If
// any of them are not the halo color, returns true. This defines the halo of
// pixels that will appear around the text. Note that we have to check each
// pixel against both the halo color and transparent since |DrawStringWithHalo|
// will modify the bitmap as it goes, and cleared pixels shouldn't count as
// changed.
bool PixelShouldGetHalo(const SkBitmap& bitmap,
                        int x, int y,
                        SkColor halo_color) {
  if (x > 0 &&
      *bitmap.getAddr32(x - 1, y) != halo_color &&
      *bitmap.getAddr32(x - 1, y) != 0)
    return true;  // Touched pixel to the left.
  if (x < bitmap.width() - 1 &&
      *bitmap.getAddr32(x + 1, y) != halo_color &&
      *bitmap.getAddr32(x + 1, y) != 0)
    return true;  // Touched pixel to the right.
  if (y > 0 &&
      *bitmap.getAddr32(x, y - 1) != halo_color &&
      *bitmap.getAddr32(x, y - 1) != 0)
    return true;  // Touched pixel above.
  if (y < bitmap.height() - 1 &&
      *bitmap.getAddr32(x, y + 1) != halo_color &&
      *bitmap.getAddr32(x, y + 1) != 0)
    return true;  // Touched pixel below.
  return false;
}

// Apply vertical alignment per |flags|. Returns y-coordinate delta.
int VAlignText(const gfx::Font& font,
               int line_count,
               int flags,
               int available_height) {
  const int text_size = font.GetFontSize();

  if (flags & gfx::Canvas::TEXT_VALIGN_TOP)
    return text_size;

  if (flags & gfx::Canvas::TEXT_VALIGN_BOTTOM) {
    // Note: The -1 was chosen empirically to match the existing GDI code.
    int offset = available_height + text_size - font.GetHeight() - 1;
    if (line_count > 1)
      offset -= (line_count * text_size);
    return offset;
  }

  // Default case: TEXT_VALIGN_MIDDLE.
  // Note: The +1 below and the -2 further down were chosen empirically to match
  // the alignment and rounding in the existing GDI code.
  int double_offset = available_height + text_size + 1;
  if (line_count > 1)
    double_offset -= (line_count * text_size);
  return double_offset / 2 - 2;
}

// Updates |render_text| from the specified parameters.
void UpdateRenderText(gfx::RenderText* render_text,
                      const gfx::Rect& rect,
                      const string16& text,
                      const gfx::Font& font,
                      int flags,
                      SkColor color) {
  int accelerated_char_pos = -1;
  int accelerated_char_span = 0;
  string16 transformed_text = text;

  // Strip accelerator character prefixes.
  if (flags & (gfx::Canvas::SHOW_PREFIX | gfx::Canvas::HIDE_PREFIX)) {
    transformed_text = gfx::RemoveAcceleratorChar(text,
                                                  '&',
                                                  &accelerated_char_pos,
                                                  &accelerated_char_span);
  }

  render_text->SetFontList(gfx::FontList(font));
  render_text->SetText(transformed_text);
  render_text->SetCursorEnabled(false);

  gfx::Rect display_rect = rect;
  display_rect.Offset(0, -font.GetFontSize());
  display_rect.set_height(font.GetHeight());
  render_text->SetDisplayRect(display_rect);

  if (flags & gfx::Canvas::TEXT_ALIGN_RIGHT)
    render_text->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  else if (flags & gfx::Canvas::TEXT_ALIGN_CENTER)
    render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  else
    render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  gfx::StyleRange style;
  style.foreground = color;
  style.font_style = font.GetStyle();
  if (font.GetStyle() & gfx::Font::UNDERLINED)
    style.underline = true;
  render_text->set_default_style(style);
  render_text->ApplyDefaultStyle();

  // Underline the accelerator char, if present.
  if ((flags & gfx::Canvas::SHOW_PREFIX) && accelerated_char_pos != -1 &&
      !style.underline) {
    gfx::StyleRange underline_style = style;
    int accelerated_char_end = accelerated_char_pos + accelerated_char_span;
    underline_style.range.set_start(accelerated_char_pos);
    underline_style.range.set_end(accelerated_char_end);
    underline_style.underline = true;
    render_text->ApplyStyleRange(underline_style);
  }
}

}  // anonymous namespace

namespace gfx {

// static
void CanvasSkia::SizeStringInt(const string16& text,
                               const gfx::Font& font,
                               int* width, int* height,
                               int flags) {
  DCHECK_GE(*width, 0);
  DCHECK_GE(*height, 0);

  if ((flags & MULTI_LINE) && *width != 0) {
    ui::WordWrapBehavior wrap_behavior = ui::TRUNCATE_LONG_WORDS;
    if (flags & CHARACTER_BREAK)
      wrap_behavior = ui::WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ui::ELIDE_LONG_WORDS;

    gfx::Rect rect(*width, INT_MAX);
    std::vector<string16> strings;
    ui::ElideRectangleText(text, font, rect.width(), rect.height(),
                           wrap_behavior, &strings);
    scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
    UpdateRenderText(render_text.get(), rect, string16(), font, flags, 0);

    int h = 0;
    int w = 0;
    for (size_t i = 0; i < strings.size(); ++i) {
      if (flags & (SHOW_PREFIX | HIDE_PREFIX))
        strings[i] = gfx::RemoveAcceleratorChar(strings[i], '&', NULL, NULL);
      render_text->SetText(strings[i]);
      w = std::max(w, render_text->GetStringWidth());
      h += font.GetHeight();
    }
    *width = w;
    *height = h;
  } else {
    // If the string is too long, the call by |RenderTextWin| to |ScriptShape()|
    // will inexplicably fail with result E_INVALIDARG. Guard against this.
    const size_t kMaxRenderTextLength = 5000;
    if (text.length() >= kMaxRenderTextLength) {
      *width = text.length() * font.GetAverageCharacterWidth();
    } else {
      scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
      gfx::Rect rect(*width, *height);
      UpdateRenderText(render_text.get(), rect, text, font, flags, 0);
      *width = render_text->GetStringWidth();
    }
    *height = font.GetHeight();
  }
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               int x, int y, int w, int h,
                               int flags) {
  if (!IntersectsClipRectInt(x, y, w, h))
    return;

  // TODO(asvitkine): On Windows, MULTI_LINE implies top alignment.
  //                  http://crbug.com/107357
  if (flags & MULTI_LINE) {
    flags &= ~(TEXT_VALIGN_MIDDLE | TEXT_VALIGN_BOTTOM);
    flags |= TEXT_VALIGN_TOP;
  }

  gfx::Rect rect(x, y, w, h);
  canvas_->save(SkCanvas::kClip_SaveFlag);
  ClipRect(rect);

  string16 adjusted_text = text;
  if (IsTextRTL(flags, text))
    base::i18n::AdjustStringForLocaleDirection(&adjusted_text);

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());

  if (flags & MULTI_LINE) {
    ui::WordWrapBehavior wrap_behavior = ui::IGNORE_LONG_WORDS;
    if (flags & CHARACTER_BREAK)
      wrap_behavior = ui::WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ui::ELIDE_LONG_WORDS;

    std::vector<string16> strings;
    ui::ElideRectangleText(adjusted_text, font, w, h, wrap_behavior,
                           &strings);

    rect.Offset(0, VAlignText(font, strings.size(), flags, h));
    for (size_t i = 0; i < strings.size(); i++) {
      UpdateRenderText(render_text.get(), rect, strings[i], font, flags, color);
      render_text->Draw(this);
      rect.Offset(0, font.GetHeight());
    }
  } else {
    if (!(flags & NO_ELLIPSIS))
      adjusted_text = ui::ElideText(adjusted_text, font, w, ui::ELIDE_AT_END);

    rect.Offset(0, VAlignText(font, 1, flags, h));
    UpdateRenderText(render_text.get(), rect, adjusted_text, font, flags,
                     color);
    render_text->Draw(this);
  }

  canvas_->restore();
}

void CanvasSkia::DrawStringWithHalo(const string16& text,
                                    const gfx::Font& font,
                                    const SkColor& text_color,
                                    const SkColor& halo_color_in,
                                    int x, int y, int w, int h,
                                    int flags) {
  // Some callers will have semitransparent halo colors, which we don't handle
  // (since the resulting image can have 1-bit transparency only).
  SkColor halo_color = halo_color_in | 0xFF000000;

  // Create a temporary buffer filled with the halo color. It must leave room
  // for the 1-pixel border around the text.
  gfx::Size size(w + 2, h + 2);
  CanvasSkia text_canvas(size, true);
  SkPaint bkgnd_paint;
  bkgnd_paint.setColor(halo_color);
  text_canvas.DrawRect(gfx::Rect(size), bkgnd_paint);

  // Draw the text into the temporary buffer. This will have correct
  // ClearType since the background color is the same as the halo color.
  text_canvas.DrawStringInt(text, font, text_color, 1, 1, w, h, flags);

  uint32_t halo_premul = SkPreMultiplyColor(halo_color);
  SkBitmap& text_bitmap = const_cast<SkBitmap&>(
      skia::GetTopDevice(*text_canvas.sk_canvas())->accessBitmap(true));

  for (int cur_y = 0; cur_y < h + 2; cur_y++) {
    uint32_t* text_row = text_bitmap.getAddr32(0, cur_y);
    for (int cur_x = 0; cur_x < w + 2; cur_x++) {
      if (text_row[cur_x] == halo_premul) {
        // This pixel was not touched by the text routines. See if it borders
        // a touched pixel in any of the 4 directions (not diagonally).
        if (!PixelShouldGetHalo(text_bitmap, cur_x, cur_y, halo_premul))
          text_row[cur_x] = 0;  // Make transparent.
      } else {
        text_row[cur_x] |= 0xff << SK_A32_SHIFT;  // Make opaque.
      }
    }
  }

  // Draw the halo bitmap with blur.
  DrawBitmapInt(text_bitmap, x - 1, y - 1);
}

void CanvasSkia::DrawFadeTruncatingString(
      const string16& text,
      CanvasSkia::TruncateFadeMode truncate_mode,
      size_t desired_characters_to_truncate_from_head,
      const gfx::Font& font,
      const SkColor& color,
      const gfx::Rect& display_rect) {
  int flags = NO_ELLIPSIS;

  // If the whole string fits in the destination then just draw it directly.
  if (GetStringWidth(text, font) <= display_rect.width()) {
    DrawStringInt(text, font, color, display_rect.x(), display_rect.y(),
                  display_rect.width(), display_rect.height(), flags);
    return;
  }

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  string16 clipped_text = text;
  const bool is_rtl = IsTextRTL(flags, text);
  if (is_rtl)
    base::i18n::AdjustStringForLocaleDirection(&clipped_text);

  switch (truncate_mode) {
    case TruncateFadeTail:
      render_text->set_fade_tail(true);
      if (is_rtl)
        flags |= TEXT_ALIGN_RIGHT;
      break;
    case TruncateFadeHead:
      render_text->set_fade_head(true);
      if (!is_rtl)
        flags |= TEXT_ALIGN_RIGHT;
      break;
    case TruncateFadeHeadAndTail:
      DCHECK_GT(desired_characters_to_truncate_from_head, 0u);
      // Due to the fade effect the first character is hard to see.
      // We want to make sure that the first character starting at
      // |desired_characters_to_truncate_from_head| is readable so we reduce
      // the offset by a little bit.
      desired_characters_to_truncate_from_head =
          std::max<int>(0, desired_characters_to_truncate_from_head - 2);

      if (desired_characters_to_truncate_from_head) {
        // Make sure to clip the text at a UTF16 boundary.
        U16_SET_CP_LIMIT(text.data(), 0,
                         desired_characters_to_truncate_from_head,
                         text.length());
        clipped_text = text.substr(desired_characters_to_truncate_from_head);
      }

      render_text->set_fade_tail(true);
      render_text->set_fade_head(true);
      break;
  }

  gfx::Rect text_rect = display_rect;
  text_rect.Offset(0, VAlignText(font, 1, flags, display_rect.height()));
  UpdateRenderText(render_text.get(), text_rect, clipped_text, font, flags,
                   color);

  canvas_->save(SkCanvas::kClip_SaveFlag);
  ClipRect(display_rect);
  render_text->Draw(this);
  canvas_->restore();
}

}  // namespace gfx
