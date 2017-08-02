/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebFontRenderStyle_h
#define WebFontRenderStyle_h

#include "public/platform/WebCommon.h"

namespace blink {

struct FontRenderStyle;

struct BLINK_EXPORT WebFontRenderStyle {
  // Each of the use* members below can take one of three values:
  //   0: off
  //   1: on
  //   2: no preference expressed
  char use_bitmaps;               // use embedded bitmap strike if possible
  char use_auto_hint;             // use 'auto' hinting (FreeType specific)
  char use_hinting;               // hint glyphs to the pixel grid
  char hint_style;                // level of hinting, 0..3
  char use_anti_alias;            // antialias glyph shapes
  char use_subpixel_rendering;    // use subpixel rendering (partially-filled
                                  // pixels)
  char use_subpixel_positioning;  // use subpixel positioning (fractional X
                                  // positions for glyphs)

#if INSIDE_BLINK
  // Translates the members of this struct to a FontRenderStyle
  void ToFontRenderStyle(FontRenderStyle*);
#endif

  void SetDefaults();
};

}  // namespace blink

#endif  // WebFontRenderStyle_h
