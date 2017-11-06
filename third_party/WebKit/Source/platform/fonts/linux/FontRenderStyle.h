/* Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontRenderStyle_h
#define FontRenderStyle_h

#include "SkFontStyle.h"
#include "SkPaint.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/paint/PaintFont.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/CString.h"

namespace blink {

// FontRenderStyle describes the user's preferences for rendering a font at a
// given size.
struct FontRenderStyle {
  DISALLOW_NEW();
  enum {
    kNoPreference = 2,
  };

  FontRenderStyle()
      : use_bitmaps(0),
        use_auto_hint(0),
        use_hinting(0),
        hint_style(0),
        use_anti_alias(0),
        use_subpixel_rendering(0),
        use_subpixel_positioning(0) {}

  bool operator==(const FontRenderStyle& a) const {
    return use_bitmaps == a.use_bitmaps && use_auto_hint == a.use_auto_hint &&
           use_hinting == a.use_hinting && hint_style == a.hint_style &&
           use_anti_alias == a.use_anti_alias &&
           use_subpixel_rendering == a.use_subpixel_rendering &&
           use_subpixel_positioning == a.use_subpixel_positioning;
  }

  PLATFORM_EXPORT static void SetHinting(SkPaint::Hinting);
  PLATFORM_EXPORT static void SetAutoHint(bool);
  PLATFORM_EXPORT static void SetUseBitmaps(bool);
  PLATFORM_EXPORT static void SetAntiAlias(bool);
  PLATFORM_EXPORT static void SetSubpixelRendering(bool);

  static FontRenderStyle QuerySystem(const CString& family,
                                     float text_size,
                                     SkFontStyle);
  void ApplyToPaintFont(PaintFont&, float device_scale_factor) const;

  // Each of the use* members below can take one of three values:
  //   0: off
  //   1: on
  //   NoPreference: no preference expressed
  char use_bitmaps;     // use embedded bitmap strike if possible
  char use_auto_hint;   // use 'auto' hinting (FreeType specific)
  char use_hinting;     // hint glyphs to the pixel grid
  char hint_style;      // level of hinting, 0..3
  char use_anti_alias;  // antialias glyph shapes
  // use subpixel rendering (partially-filled pixels)
  char use_subpixel_rendering;
  // use subpixel positioning (fractional X positions for glyphs)
  char use_subpixel_positioning;
};

}  // namespace blink

#endif  // FontRenderStyle_h
