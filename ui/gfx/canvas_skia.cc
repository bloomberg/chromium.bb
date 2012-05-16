// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/range/range.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_util.h"

namespace {

// If necessary, wraps |text| with RTL/LTR directionality characters based on
// |flags| and |text| content.
// Returns true if the text will be rendered right-to-left.
// TODO(asvitkine): Support setting directionality directly on RenderText, so
//                  that wrapping the text is not needed.
bool AdjustStringDirection(int flags, string16* text) {
  // If the string is empty or LTR was forced, simply return false since the
  // default RenderText directionality is already LTR.
  if (text->empty() || (flags & gfx::Canvas::FORCE_LTR_DIRECTIONALITY))
    return false;

  // If RTL is forced, apply it to the string.
  if (flags & gfx::Canvas::FORCE_RTL_DIRECTIONALITY) {
    base::i18n::WrapStringWithRTLFormatting(text);
    return true;
  }

  // If a direction wasn't forced but the UI language is RTL and there were
  // strong RTL characters, ensure RTL is applied.
  if (base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(*text)) {
    base::i18n::WrapStringWithRTLFormatting(text);
    return true;
  }

  // In the default case, the string should be rendered as LTR. RenderText's
  // default directionality is LTR, so the text doesn't need to be wrapped.
  // Note that individual runs within the string may still be rendered RTL
  // (which will be the case for RTL text under non-RTL locales, since under RTL
  // locales it will be handled by the if statement above).
  return false;
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
int VAlignText(int text_height,
               int flags,
               int available_height) {
  if (flags & gfx::Canvas::TEXT_VALIGN_TOP)
    return 0;

  if (flags & gfx::Canvas::TEXT_VALIGN_BOTTOM)
    return available_height - text_height;

  // Default case: TEXT_VALIGN_MIDDLE.
  return (available_height - text_height) / 2;
}

// Strips accelerator character prefixes in |text| if needed, based on |flags|.
// Returns a range in |text| to underline or ui::Range::InvalidRange() if
// underlining is not needed.
ui::Range StripAcceleratorChars(int flags, string16* text) {
  if (flags & (gfx::Canvas::SHOW_PREFIX | gfx::Canvas::HIDE_PREFIX)) {
    int char_pos = -1;
    int char_span = 0;
    *text = gfx::RemoveAcceleratorChar(*text, '&', &char_pos, &char_span);
    if ((flags & gfx::Canvas::SHOW_PREFIX) && char_pos != -1)
      return ui::Range(char_pos, char_pos + char_span);
  }
  return ui::Range::InvalidRange();
}

// Elides |text| and adjusts |range| appropriately. If eliding causes |range|
// to no longer point to the same character in |text|, |range| is made invalid.
void ElideTextAndAdjustRange(const gfx::Font& font,
                             int width,
                             string16* text,
                             ui::Range* range) {
  const char16 start_char = (range->IsValid() ? text->at(range->start()) : 0);
  *text = ui::ElideText(*text, font, width, ui::ELIDE_AT_END);
  if (!range->IsValid())
    return;
  if (range->start() >= text->length() ||
      text->at(range->start()) != start_char) {
    *range = ui::Range::InvalidRange();
  }
}

// Updates |render_text| from the specified parameters.
void UpdateRenderText(const gfx::Rect& rect,
                      const string16& text,
                      const gfx::Font& font,
                      int flags,
                      SkColor color,
                      gfx::RenderText* render_text) {
  render_text->SetFontList(gfx::FontList(font));
  render_text->SetText(text);
  render_text->SetCursorEnabled(false);

  gfx::Rect display_rect = rect;
  display_rect.set_height(font.GetHeight());
  render_text->SetDisplayRect(display_rect);

  if (flags & gfx::Canvas::TEXT_ALIGN_RIGHT)
    render_text->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  else if (flags & gfx::Canvas::TEXT_ALIGN_CENTER)
    render_text->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  else
    render_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (flags & gfx::Canvas::NO_SUBPIXEL_RENDERING)
    render_text->set_background_is_transparent(true);

  gfx::StyleRange style;
  style.foreground = color;
  style.font_style = font.GetStyle();
  if (font.GetStyle() & gfx::Font::UNDERLINED)
    style.underline = true;
  render_text->set_default_style(style);
  render_text->ApplyDefaultStyle();
}

// Adds an underline style to |render_text| over |range|.
void ApplyUnderlineStyle(const ui::Range& range, gfx::RenderText* render_text) {
  gfx::StyleRange style = render_text->default_style();
  if (range.IsValid() && !style.underline) {
    style.range = range;
    style.underline = true;
    render_text->ApplyStyleRange(style);
  }
}

// Returns updated |flags| to match platform-specific expected behavior.
int AdjustPlatformSpecificFlags(const string16& text, int flags) {
#if defined(OS_LINUX)
  // TODO(asvitkine): On Linux, NO_ELLIPSIS really means MULTI_LINE.
  //                  http://crbug.com/107357
  if (flags & gfx::Canvas::NO_ELLIPSIS)
    flags |= gfx::Canvas::MULTI_LINE;

  // TODO(asvitkine): ash/tooltips/tooltip_controller.cc adds \n's to the string
  //                  without passing MULTI_LINE.
  if (text.find('\n') != string16::npos)
    flags |= gfx::Canvas::MULTI_LINE;
#endif

  return flags;
}

}  // anonymous namespace

namespace gfx {

// static
void Canvas::SizeStringInt(const string16& text,
                           const gfx::Font& font,
                           int* width, int* height,
                           int flags) {
  DCHECK_GE(*width, 0);
  DCHECK_GE(*height, 0);

  flags = AdjustPlatformSpecificFlags(text, flags);

  string16 adjusted_text = text;
#if defined(OS_WIN)
  AdjustStringDirection(flags, &adjusted_text);
#endif

  if ((flags & MULTI_LINE) && *width != 0) {
    ui::WordWrapBehavior wrap_behavior = ui::TRUNCATE_LONG_WORDS;
    if (flags & CHARACTER_BREAK)
      wrap_behavior = ui::WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ui::ELIDE_LONG_WORDS;

    gfx::Rect rect(*width, INT_MAX);
    std::vector<string16> strings;
    ui::ElideRectangleText(adjusted_text, font, rect.width(), rect.height(),
                           wrap_behavior, &strings);
    scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
    UpdateRenderText(rect, string16(), font, flags, 0, render_text.get());

    int h = 0;
    int w = 0;
    for (size_t i = 0; i < strings.size(); ++i) {
      StripAcceleratorChars(flags, &strings[i]);
      render_text->SetText(strings[i]);
      const Size string_size = render_text->GetStringSize();
      w = std::max(w, string_size.width());
      h += string_size.height();
    }
    *width = w;
    *height = h;
  } else {
    // If the string is too long, the call by |RenderTextWin| to |ScriptShape()|
    // will inexplicably fail with result E_INVALIDARG. Guard against this.
    const size_t kMaxRenderTextLength = 5000;
    if (adjusted_text.length() >= kMaxRenderTextLength) {
      *width = adjusted_text.length() * font.GetAverageCharacterWidth();
      *height = font.GetHeight();
    } else {
      scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
      gfx::Rect rect(*width, *height);
      StripAcceleratorChars(flags, &adjusted_text);
      UpdateRenderText(rect, adjusted_text, font, flags, 0, render_text.get());
      const Size string_size = render_text->GetStringSize();
      *width = string_size.width();
      *height = string_size.height();
    }
  }
}

void Canvas::DrawStringWithShadows(const string16& text,
                                   const gfx::Font& font,
                                   SkColor color,
                                   const gfx::Rect& text_bounds,
                                   int flags,
                                   const std::vector<ShadowValue>& shadows) {
  if (!IntersectsClipRect(text_bounds))
    return;

  flags = AdjustPlatformSpecificFlags(text, flags);

#if defined(OS_WIN)
  // TODO(asvitkine): On Windows, MULTI_LINE implies top alignment.
  //                  http://crbug.com/107357
  if (flags & MULTI_LINE) {
    flags &= ~(TEXT_VALIGN_MIDDLE | TEXT_VALIGN_BOTTOM);
    flags |= TEXT_VALIGN_TOP;
  }
#endif

  gfx::Rect clip_rect(text_bounds);
  clip_rect.Inset(ShadowValue::GetMargin(shadows));

  canvas_->save(SkCanvas::kClip_SaveFlag);
  ClipRect(clip_rect);

  gfx::Rect rect(text_bounds);
  string16 adjusted_text = text;

#if defined(OS_WIN)
  AdjustStringDirection(flags, &adjusted_text);
#endif

  scoped_ptr<RenderText> render_text(RenderText::CreateRenderText());
  render_text->SetTextShadows(shadows);

  if (flags & MULTI_LINE) {
    ui::WordWrapBehavior wrap_behavior = ui::IGNORE_LONG_WORDS;
    if (flags & CHARACTER_BREAK)
      wrap_behavior = ui::WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ui::ELIDE_LONG_WORDS;

    std::vector<string16> strings;
    ui::ElideRectangleText(adjusted_text,
                           font,
                           text_bounds.width(), text_bounds.height(),
                           wrap_behavior,
                           &strings);

    for (size_t i = 0; i < strings.size(); i++) {
      ui::Range range = StripAcceleratorChars(flags, &strings[i]);
      UpdateRenderText(rect, strings[i], font, flags, color, render_text.get());

      // Apply vertical alignment over the block of text using the height of the
      // first line. This may not be correct if different lines in the text have
      // different heights, but avoids needing to do two passes.
      const int line_height = render_text->GetStringSize().height();
      if (i == 0) {
        rect.Offset(0, VAlignText(strings.size() * line_height,
                                  flags,
                                  text_bounds.height()));
      }
      rect.set_height(line_height);

      ApplyUnderlineStyle(range, render_text.get());
      render_text->Draw(this);
      rect.Offset(0, line_height);
    }
  } else {
    ui::Range range = StripAcceleratorChars(flags, &adjusted_text);
    bool elide_text = (flags & NO_ELLIPSIS) ? false : true;

#if defined(OS_LINUX)
    // On Linux, eliding really means fading the end of the string. But only
    // for LTR text. RTL text is still elided (on the left) with "...".
    if (elide_text) {
      render_text->SetText(adjusted_text);
      if (render_text->GetTextDirection() == base::i18n::LEFT_TO_RIGHT) {
        render_text->set_fade_tail(true);
        elide_text = false;
      }
    }
#endif

    if (elide_text) {
      ElideTextAndAdjustRange(font,
                              text_bounds.width(),
                              &adjusted_text,
                              &range);
    }

    UpdateRenderText(rect, adjusted_text, font, flags, color,
                     render_text.get());

    const int line_height = render_text->GetStringSize().height();
    rect.Offset(0, VAlignText(line_height, flags, text_bounds.height()));
    rect.set_height(line_height);
    render_text->SetDisplayRect(rect);

    ApplyUnderlineStyle(range, render_text.get());
    render_text->Draw(this);
  }

  canvas_->restore();
}

void Canvas::DrawStringWithHalo(const string16& text,
                                const gfx::Font& font,
                                SkColor text_color,
                                SkColor halo_color_in,
                                int x, int y, int w, int h,
                                int flags) {
  // Some callers will have semitransparent halo colors, which we don't handle
  // (since the resulting image can have 1-bit transparency only).
  SkColor halo_color = SkColorSetA(halo_color_in, 0xFF);

  // Create a temporary buffer filled with the halo color. It must leave room
  // for the 1-pixel border around the text.
  Size size(w + 2, h + 2);
  Canvas text_canvas(size, true);
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

// TODO(asvitkine): Remove the ifdef once all platforms use canvas_skia.cc.
#if defined(OS_WIN)
void Canvas::DrawFadeTruncatingString(
      const string16& text,
      TruncateFadeMode truncate_mode,
      size_t desired_characters_to_truncate_from_head,
      const gfx::Font& font,
      SkColor color,
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
  const bool is_rtl = AdjustStringDirection(flags, &clipped_text);

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

  gfx::Rect rect = display_rect;
  UpdateRenderText(rect, clipped_text, font, flags, color, render_text.get());

  const int line_height = render_text->GetStringSize().height();
  rect.Offset(0, VAlignText(line_height, flags, display_rect.height()));
  rect.set_height(line_height);
  render_text->SetDisplayRect(rect);

  canvas_->save(SkCanvas::kClip_SaveFlag);
  ClipRect(display_rect);
  render_text->Draw(this);
  canvas_->restore();
}
#endif

}  // namespace gfx
