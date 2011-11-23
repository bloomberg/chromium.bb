// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas_skia.h"

#include <limits>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_gdi_object.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"

namespace {

static inline int Round(double x) {
  // Why oh why is this not in a standard header?
  return static_cast<int>(floor(x + 0.5));
}

// We make sure that LTR text we draw in an RTL context is modified
// appropriately to make sure it maintains it LTR orientation.
void DoDrawText(HDC hdc,
                const string16& text,
                RECT* text_bounds,
                int flags) {
  // Only adjust string directionality if both of the following are true:
  // 1. The current locale is RTL.
  // 2. The string itself has RTL directionality.
  const wchar_t* string_ptr = text.c_str();
  int string_size = static_cast<int>(text.length());

  string16 localized_text;
  if (flags & DT_RTLREADING) {
    localized_text = text;
    base::i18n::AdjustStringForLocaleDirection(&localized_text);
    string_ptr = localized_text.c_str();
    string_size = static_cast<int>(localized_text.length());
  }

  DrawText(hdc, string_ptr, string_size, text_bounds, flags);
}

// Compute the windows flags necessary to implement the provided text Canvas
// flags.
int ComputeFormatFlags(int flags, const string16& text) {
  // Setting the text alignment explicitly in case it hasn't already been set.
  // This will make sure that we don't align text to the left on RTL locales
  // just because no alignment flag was passed to DrawStringInt().
  if (!(flags & (gfx::Canvas::TEXT_ALIGN_CENTER |
                 gfx::Canvas::TEXT_ALIGN_RIGHT |
                 gfx::Canvas::TEXT_ALIGN_LEFT))) {
    flags |= gfx::CanvasSkia::DefaultCanvasTextAlignment();
  }

  // horizontal alignment
  int f = 0;
  if (flags & gfx::Canvas::TEXT_ALIGN_CENTER)
    f |= DT_CENTER;
  else if (flags & gfx::Canvas::TEXT_ALIGN_RIGHT)
    f |= DT_RIGHT;
  else
    f |= DT_LEFT;

  // vertical alignment
  if (flags & gfx::Canvas::TEXT_VALIGN_TOP)
    f |= DT_TOP;
  else if (flags & gfx::Canvas::TEXT_VALIGN_BOTTOM)
    f |= DT_BOTTOM;
  else
    f |= DT_VCENTER;

  if (flags & gfx::Canvas::MULTI_LINE) {
    f |= DT_WORDBREAK;
    if (flags & gfx::Canvas::CHARACTER_BREAK)
      f |= DT_EDITCONTROL;  // Turns on character breaking (not documented)
    else if (!(flags & gfx::Canvas::NO_ELLIPSIS))
      f |= DT_WORD_ELLIPSIS;
  } else {
    f |= DT_SINGLELINE;
  }

  if (flags & gfx::Canvas::HIDE_PREFIX)
    f |= DT_HIDEPREFIX;
  else if ((flags & gfx::Canvas::SHOW_PREFIX) == 0)
    f |= DT_NOPREFIX;

  if (!(flags & gfx::Canvas::NO_ELLIPSIS))
    f |= DT_END_ELLIPSIS;

  // In order to make sure RTL/BiDi strings are rendered correctly, we must
  // pass the flag DT_RTLREADING to DrawText (when the locale's language is
  // a right-to-left language) so that Windows does the right thing.
  //
  // In addition to correctly displaying text containing both RTL and LTR
  // elements (for example, a string containing a telephone number within a
  // sentence in Hebrew, or a sentence in Hebrew that contains a word in
  // English) this flag also makes sure that if there is not enough space to
  // display the entire string, the ellipsis is displayed on the left hand side
  // of the truncated string and not on the right hand side.
  //
  // We make a distinction between Chrome UI strings and text coming from a web
  // page.
  //
  // For text coming from a web page we determine the alignment based on the
  // first character with strong directionality. If the directionality of the
  // first character with strong directionality in the text is LTR, the
  // alignment is set to DT_LEFT, and the directionality should not be set as
  // DT_RTLREADING. If the directionality of the first character with strong
  // directionality in the text is RTL, its alignment is set to DT_RIGHT, and
  // its directionality is set as DT_RTLREADING through
  // FORCE_RTL_DIRECTIONALITY.
  //
  // This heuristic doesn't work for Chrome UI strings since even in RTL
  // locales, some of those might start with English text but we know they're
  // localized so their directionality should be set as DT_RTLREADING if it
  // contains strong RTL characters.
  //
  // Caveat: If the string is purely LTR, don't set DTL_RTLREADING since when
  // the flag is set, LRE-PDF don't have the desired effect of rendering
  // multiline English-only text as LTR.
  //
  // Note that if the caller is explicitly requesting displaying the text
  // using RTL directionality then we respect that and pass DT_RTLREADING to
  // ::DrawText even if the locale is LTR.
  int force_rtl = (flags & gfx::Canvas::FORCE_RTL_DIRECTIONALITY);
  int force_ltr = (flags & gfx::Canvas::FORCE_LTR_DIRECTIONALITY);
  bool is_rtl = base::i18n::IsRTL();
  bool string_contains_strong_rtl_chars =
      base::i18n::StringContainsStrongRTLChars(text);
  if (force_rtl || (!force_ltr && is_rtl && string_contains_strong_rtl_chars))
    f |= DT_RTLREADING;

  return f;
}

// Changes the alpha of the given bitmap.
// If |fade_to_right| is true then the rect fades from opaque to clear,
// otherwise the rect fades from clear to opaque.
void FadeBitmapRect(SkDevice& bmp_device,
                    const gfx::Rect& rect,
                    bool fade_to_right) {
  SkBitmap bmp = bmp_device.accessBitmap(true);
  DCHECK_EQ(SkBitmap::kARGB_8888_Config, bmp.config());
  SkAutoLockPixels lock(bmp);
  float total_width = static_cast<float>(rect.width());

  for (int x = rect.x(); x < rect.right(); x++) {
    float cur_width = static_cast<float>(fade_to_right ?
        rect.right() - x :  x - rect.x());
    // We want the fade effect to go from 0.2 to 1.0.
    float alpha_percent = ((cur_width / total_width) * 0.8f) + 0.2f;

    for (int y = rect.y(); y < rect.bottom(); y++) {
      SkColor color = bmp.getColor(x, y);
      SkAlpha alpha = static_cast<SkAlpha>(SkColorGetA(color) * alpha_percent);
      *bmp.getAddr32(x, y) = SkPreMultiplyColor(SkColorSetA(color, alpha));
    }
  }
}

// DrawText() doesn't support alpha channels. To create a transparent background
// this function draws black on white. It then uses the intensity of black
// to determine how much alpha to use. The text is drawn in |gfx_text_rect| and
// clipped to |gfx_draw_rect|.
void DrawTextAndClearBackground(SkCanvas* bmp_canvas,
                                HFONT font,
                                COLORREF text_color,
                                const string16& text,
                                int flags,
                                const gfx::Rect& gfx_text_rect,
                                const gfx::Rect& gfx_draw_rect) {
  skia::ScopedPlatformPaint scoped_platform_paint(bmp_canvas);
  HDC hdc = scoped_platform_paint.GetPlatformSurface();

  // Clear the background by filling with white.
  HBRUSH fill_brush = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  HANDLE old_brush = SelectObject(hdc, fill_brush);
  RECT draw_rect = gfx_draw_rect.ToRECT();
  FillRect(hdc, &draw_rect, fill_brush);
  SelectObject(hdc, old_brush);

  // Set black text with trasparent background.
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, 0);

  // Draw the text.
  int save_dc_id = SaveDC(hdc);
  // Clip the text to the draw destination.
  IntersectClipRect(hdc, draw_rect.left, draw_rect.top,
                    draw_rect.right, draw_rect.bottom);
  SelectObject(hdc, font);
  RECT text_rect = gfx_text_rect.ToRECT();
  DoDrawText(hdc, text, &text_rect,
             ComputeFormatFlags(flags, text));
  RestoreDC(hdc, save_dc_id);

  BYTE text_color_r = GetRValue(text_color);
  BYTE text_color_g = GetGValue(text_color);
  BYTE text_color_b = GetBValue(text_color);

  SkBitmap bmp = bmp_canvas->getTopDevice()->accessBitmap(true);
  DCHECK_EQ(SkBitmap::kARGB_8888_Config, bmp.config());
  SkAutoLockPixels lock(bmp);

  // At this point the bitmap has black text on white.
  // The intensity of black tells us the alpha value of the text.
  for (int y = draw_rect.top; y < draw_rect.bottom; y++) {
    for (int x = draw_rect.left; x < draw_rect.right; x++) {
      // Gets the color directly. DrawText doesn't premultiply alpha so
      // using SkBitmap::getColor() won't work here.
      SkColor color = *bmp.getAddr32(x, y);
      // Calculate the alpha using the luminance. Since this is black text
      // on a white background the luminosity must be inverted.
      BYTE alpha = 0xFF - color_utils::GetLuminanceForColor(color);
      *bmp.getAddr32(x, y) = SkPreMultiplyColor(
          SkColorSetARGB(alpha, text_color_r, text_color_g, text_color_b));
    }
  }
}

// Draws the given text with a fade out gradient. |bmp_device| is a bitmap
// that is used to temporary drawing. The text is drawn in |text_rect| and
// clipped to |draw_rect|.
void DrawTextGradientPart(HDC hdc,
                          SkCanvas* bmp_canvas,
                          const string16& text,
                          const SkColor& color,
                          HFONT font,
                          const gfx::Rect& text_rect,
                          const gfx::Rect& draw_rect,
                          bool fade_to_right,
                          int flags) {
  DrawTextAndClearBackground(bmp_canvas, font, skia::SkColorToCOLORREF(color),
                             text, flags, text_rect, draw_rect);
  FadeBitmapRect(*bmp_canvas->getTopDevice(), draw_rect, fade_to_right);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

  skia::ScopedPlatformPaint scoped_platform_paint(bmp_canvas);
  HDC bmp_hdc = scoped_platform_paint.GetPlatformSurface();
  AlphaBlend(hdc, draw_rect.x(), draw_rect.y(), draw_rect.width(),
             draw_rect.height(), bmp_hdc, draw_rect.x(), draw_rect.y(),
             draw_rect.width(), draw_rect.height(), blend);
}

enum PrimarySide {
  PrimaryOnLeft,
  PrimaryOnRight,
};

// Divides |rect| horizontally into a |primary| of width |primary_width| and a
// |secondary| taking up the remainder.
void DivideRect(const gfx::Rect& rect,
                PrimarySide primary_side,
                int primary_width,
                gfx::Rect* primary,
                gfx::Rect* secondary) {
  *primary = rect;
  *secondary = rect;
  int remainder = rect.width() - primary_width;

  switch (primary_side) {
    case PrimaryOnLeft:
      primary->Inset(0, 0, remainder, 0);
      secondary->Inset(primary_width, 0, 0, 0);
      break;
    case PrimaryOnRight:
      primary->Inset(remainder, 0, 0, 0);
      secondary->Inset(0, 0, primary_width, 0);
      break;
  }
}

}  // anonymous namespace

namespace gfx {

// static
void CanvasSkia::SizeStringInt(const string16& text,
                               const gfx::Font& font,
                               int* width, int* height,
                               int flags) {
  // Clamp the max amount of text we'll measure to 2K.  When the string is
  // actually drawn, it will be clipped to whatever size box is provided, and
  // the time to do that doesn't depend on the length being clipped off.
  const int kMaxStringLength = 2048 - 1;  // So the trailing \0 fits in 2K.
  string16 clamped_string(text.substr(0, kMaxStringLength));

  if (*width == 0) {
    // If multi-line + character break are on, the computed width will be one
    // character wide (useless).  Furthermore, if in this case the provided text
    // contains very long "words" (substrings without a word-breaking point),
    // DrawText() can run extremely slowly (e.g. several seconds).  So in this
    // case, we turn character breaking off to get a more accurate "desired"
    // width and avoid the slowdown.
    if (flags & (gfx::Canvas::MULTI_LINE | gfx::Canvas::CHARACTER_BREAK))
      flags &= ~gfx::Canvas::CHARACTER_BREAK;

    // Weird undocumented behavior: if the width is 0, DoDrawText() won't
    // calculate a size at all.  So set it to 1, which it will then change.
    if (!text.empty())
      *width = 1;
  }
  RECT r = { 0, 0, *width, *height };

  HDC dc = GetDC(NULL);
  HFONT old_font = static_cast<HFONT>(SelectObject(dc, font.GetNativeFont()));
  DoDrawText(dc, clamped_string, &r,
             ComputeFormatFlags(flags, clamped_string) | DT_CALCRECT);
  SelectObject(dc, old_font);
  ReleaseDC(NULL, dc);

  *width = r.right;
  *height = r.bottom;
}

void CanvasSkia::DrawStringInt(const string16& text,
                               HFONT font,
                               const SkColor& color,
                               int x, int y, int w, int h,
                               int flags) {
  SkRect fclip;
  if (!canvas_->getClipBounds(&fclip))
    return;
  RECT text_bounds = { x, y, x + w, y + h };
  SkIRect clip;
  fclip.round(&clip);
  if (!clip.intersect(skia::RECTToSkIRect(text_bounds)))
    return;

  // Clamp the max amount of text we'll draw to 32K.  There seem to be bugs in
  // DrawText() if you e.g. ask it to character-break a no-whitespace string of
  // length > 43680 (for which it draws nothing), and since we clamped to 2K in
  // SizeStringInt() we're unlikely to be able to display this much anyway.
  const int kMaxStringLength = 32768 - 1;  // So the trailing \0 fits in 32K.
  string16 clamped_string(text.substr(0, kMaxStringLength));

  HDC dc;
  HFONT old_font;
  {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas_);
    dc = scoped_platform_paint.GetPlatformSurface();
    SetBkMode(dc, TRANSPARENT);
    old_font = (HFONT)SelectObject(dc, font);
    COLORREF brush_color = RGB(SkColorGetR(color), SkColorGetG(color),
                               SkColorGetB(color));
    SetTextColor(dc, brush_color);

    int f = ComputeFormatFlags(flags, clamped_string);
    DoDrawText(dc, clamped_string, &text_bounds, f);
  }

  // Restore the old font. This way we don't have to worry if the caller
  // deletes the font and the DC lives longer.
  SelectObject(dc, old_font);

  // Windows will have cleared the alpha channel of the text we drew. Assume
  // we're drawing to an opaque surface, or at least the text rect area is
  // opaque.
  skia::MakeOpaque(canvas_, clip.fLeft, clip.fTop, clip.width(),
                   clip.height());
}

void CanvasSkia::DrawStringInt(const string16& text,
                               const gfx::Font& font,
                               const SkColor& color,
                               int x, int y, int w, int h,
                               int flags) {
  DrawStringInt(text, font.GetNativeFont(), color, x, y, w, h, flags);
}

// Checks each pixel immediately adjacent to the given pixel in the bitmap. If
// any of them are not the halo color, returns true. This defines the halo of
// pixels that will appear around the text. Note that we have to check each
// pixel against both the halo color and transparent since DrawStringWithHalo
// will modify the bitmap as it goes, and clears pixels shouldn't count as
// changed.
static bool pixelShouldGetHalo(const SkBitmap& bitmap,
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
  gfx::Rect rect(gfx::Point(), gfx::Size(w + 2, h + 2));
  CanvasSkia text_canvas(rect.width(), rect.height(), true);
  SkPaint bkgnd_paint;
  bkgnd_paint.setColor(halo_color);
  text_canvas.DrawRect(rect, bkgnd_paint);

  // Draw the text into the temporary buffer. This will have correct
  // ClearType since the background color is the same as the halo color.
  text_canvas.DrawStringInt(text, font, text_color, 1, 1, w, h, flags);

  // Windows will have cleared the alpha channel for the pixels it drew. Make it
  // opaque. We have to do this first since pixelShouldGetHalo will check for
  // 0 to see if a pixel has been modified to transparent, and black text that
  // Windows draw will look transparent to it!
  skia::MakeOpaque(text_canvas.sk_canvas(), rect.x(), rect.y(), rect.width(),
                   rect.height());

  uint32_t halo_premul = SkPreMultiplyColor(halo_color);
  SkBitmap& text_bitmap = const_cast<SkBitmap&>(
      skia::GetTopDevice(*text_canvas.sk_canvas())->accessBitmap(true));
  for (int cur_y = 0; cur_y < h + 2; cur_y++) {
    uint32_t* text_row = text_bitmap.getAddr32(0, cur_y);
    for (int cur_x = 0; cur_x < w + 2; cur_x++) {
      if (text_row[cur_x] == halo_premul) {
        // This pixel was not touched by the text routines. See if it borders
        // a touched pixel in any of the 4 directions (not diagonally).
        if (!pixelShouldGetHalo(text_bitmap, cur_x, cur_y, halo_premul))
          text_row[cur_x] = 0;  // Make transparent.
      } else {
        text_row[cur_x] |= 0xff << SK_A32_SHIFT;  // Make opaque.
      }
    }
  }

  // Draw the halo bitmap with blur.
  DrawBitmapInt(text_bitmap, x - 1, y - 1);
}

ui::TextureID CanvasSkia::GetTextureID() {
  // TODO(wjmaclean)
  return 0;
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
  int total_string_width;
  int total_string_height;
  SizeStringInt(text, font, &total_string_width, &total_string_height,
                flags | TEXT_VALIGN_TOP);

  if (total_string_width <= display_rect.width()) {
    DrawStringInt(text, font, color, display_rect.x(), display_rect.y(),
                  display_rect.width(), display_rect.height(), 0);
    return;
  }

  int average_character_width = font.GetAverageCharacterWidth();
  int clipped_string_width = total_string_width - display_rect.width();

  // Clip the string by drawing it to the left by |offset_x|.
  int offset_x = 0;
  switch (truncate_mode) {
    case TruncateFadeHead:
      offset_x = clipped_string_width;
      break;
    case TruncateFadeHeadAndTail:
      DCHECK_GT(desired_characters_to_truncate_from_head, 0u);
      // Get the width of the beginning of the string we're clipping.
      string16 clipped_head_string =
          text.substr(0, desired_characters_to_truncate_from_head);
      int clipped_width;
      int clipped_height;
      SizeStringInt(clipped_head_string, font,
                    &clipped_width, &clipped_height, flags);

      // This is the offset at which we start drawing. This causes the
      // beginning of the string to get clipped.
      offset_x = clipped_width;

      // Due to the fade effect the first character is hard to see.
      // We want to make sure that the first character starting at
      // |desired_characters_to_truncate_from_head| is readable so we reduce
      // the offset by a little bit.
      offset_x = std::max(0, Round(offset_x - average_character_width * 2));

      // If the offset is so large that there's empty space at the tail
      // then reduce the offset so we can use up the empty space.
      offset_x = std::min(offset_x, clipped_string_width);
      break;
  }
  bool is_truncating_head = offset_x > 0;
  bool is_truncating_tail = clipped_string_width > offset_x;

  bool is_rtl = (ComputeFormatFlags(flags, text) & DT_RTLREADING) != 0;
  // |is_rtl| tells us if the given text is right to left or not. |is_rtl| can
  // be false even if the UI is set to a right to left language.
  // Now, normally, we right align all text if the UI is set to a right to
  // left language. In this case though we don't want that because we render
  // the ends of the string ourselves.
  if (!is_rtl)
    flags |= TEXT_ALIGN_LEFT;

  // Fade in/out about 2.5 characters of the beginning/end of the string.
  // The .5 here is helpful if one of the characters is a space.
  // Use a quarter of the display width if the string is very short.
  int gradient_width = Round(std::min(average_character_width * 2.5,
                                      display_rect.width() / 4.0));

  // Move the origin to |display_rect.origin()|. This simplifies all the
  // drawing so that both the source and destination can be (0,0).
  canvas_->save(SkCanvas::kMatrix_SaveFlag);
  Translate(display_rect.origin());

  gfx::Rect solid_part(gfx::Point(), display_rect.size());
  gfx::Rect head_part;
  gfx::Rect tail_part;
  if (is_truncating_head)
    DivideRect(solid_part, is_rtl ? PrimaryOnRight : PrimaryOnLeft,
               gradient_width, &head_part, &solid_part);
  if (is_truncating_tail)
    DivideRect(solid_part, is_rtl ? PrimaryOnLeft : PrimaryOnRight,
               gradient_width, &tail_part, &solid_part);

  // Grow |display_rect| by |offset_x|.
  gfx::Rect text_rect(gfx::Point(), display_rect.size());
  if (!is_rtl)
    text_rect.set_x(text_rect.x() - offset_x);
  text_rect.set_width(text_rect.width() + offset_x);

  // Create a temporary bitmap to draw the gradient to.
  scoped_ptr<SkCanvas> gradient_canvas(skia::CreateBitmapCanvas(
      display_rect.width(), display_rect.height(), false));

  {
    skia::ScopedPlatformPaint scoped_platform_paint(canvas_);
    HDC hdc = scoped_platform_paint.GetPlatformSurface();
    if (is_truncating_head)
      DrawTextGradientPart(hdc, gradient_canvas.get(), text, color,
                           font.GetNativeFont(), text_rect, head_part, is_rtl,
                           flags);
    if (is_truncating_tail)
      DrawTextGradientPart(hdc, gradient_canvas.get(), text, color,
                           font.GetNativeFont(), text_rect, tail_part, !is_rtl,
                           flags);
  }

  // Draw the solid part.
  canvas_->save(SkCanvas::kClip_SaveFlag);
  ClipRect(solid_part);
  DrawStringInt(text, font, color,
                text_rect.x(), text_rect.y(),
                text_rect.width(), text_rect.height(),
                flags);
  canvas_->restore();
  canvas_->restore();
}

}  // namespace gfx
