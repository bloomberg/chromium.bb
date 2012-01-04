// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PANGO_UTIL_H_
#define UI_GFX_PANGO_UTIL_H_
#pragma once

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <string>

#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "ui/base/ui_export.h"
#include "third_party/skia/include/core/SkColor.h"

// TODO(xji): move other pango related functions from gtk_util to here.

namespace gfx {

class Font;
class PlatformFontPango;
class Rect;

// Uses Pango to draw text onto |cr|. This is the public method for d
void UI_EXPORT DrawTextOntoCairoSurface(cairo_t* cr,
                                        const string16& text,
                                        const gfx::Font& font,
                                        const gfx::Rect& bounds,
                                        const gfx::Rect& clip,
                                        const SkColor& text_color,
                                        int flags);

// ----------------------------------------------------------------------------
// All other methods in this file are only to be used within the ui/ directory.
// They are shared with internal skia interfaces.
// ----------------------------------------------------------------------------

// Setup pango layout |layout|, including set layout text as |text|, font
// description based on |font|, width as |width| in PANGO_SCALE for RTL lcoale,
// and set up whether auto-detect directionality, alignment, ellipsis, word
// wrapping, resolution etc.
void SetupPangoLayout(PangoLayout* layout,
                      const string16& text,
                      const gfx::Font& font,
                      int width,
                      base::i18n::TextDirection text_direction,
                      int flags);

// Setup pango layout |layout| the same way as SetupPangoLayout(), except this
// sets the font description based on |font_description|.
void SetupPangoLayoutWithFontDescription(
    PangoLayout* layout,
    const string16& text,
    const std::string& font_description,
    int width,
    base::i18n::TextDirection text_direction,
    int flags);

// Get Pango's calculated size of |layout| and modify |text_rect| within
// |bounds|.
void AdjustTextRectBasedOnLayout(PangoLayout* layout,
                                 const gfx::Rect& bounds,
                                 int flags,
                                 gfx::Rect* text_rect);

// Draws the |layout| (pango tuple of font, actual text, etc) onto |cr| using
// |text_color| as the cairo pattern.
void DrawPangoLayout(cairo_t* cr,
                     PangoLayout* layout,
                     const Font& font,
                     const gfx::Rect& bounds,
                     const gfx::Rect& text_rect,
                     const SkColor& text_color,
                     base::i18n::TextDirection text_direction,
                     int flags);

// Draw an underline under the text using |cr|, which must already be
// initialized with the correct source. |extra_edge_width| is added to the
// outer edge of the line.
void DrawPangoTextUnderline(cairo_t* cr,
                            gfx::PlatformFontPango* platform_font,
                            double extra_edge_width,
                            const Rect& text_rect);

// Returns the size in pixels for the specified |pango_font|.
size_t GetPangoFontSizeInPixels(PangoFontDescription* pango_font);

}  // namespace gfx

#endif  // UI_GFX_PANGO_UTIL_H_
