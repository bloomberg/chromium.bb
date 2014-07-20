// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_RENDER_PARAMS_H_
#define UI_GFX_FONT_RENDER_PARAMS_H_

#include <string>
#include <vector>

#include "ui/gfx/gfx_export.h"

namespace gfx {

// A collection of parameters describing how text should be rendered on Linux.
struct GFX_EXPORT FontRenderParams {
  FontRenderParams();
  ~FontRenderParams();

  // Level of hinting to be applied.
  enum Hinting {
    HINTING_NONE = 0,
    HINTING_SLIGHT,
    HINTING_MEDIUM,
    HINTING_FULL,
  };

  // Different subpixel orders to be used for subpixel rendering.
  enum SubpixelRendering {
    SUBPIXEL_RENDERING_NONE = 0,
    SUBPIXEL_RENDERING_RGB,
    SUBPIXEL_RENDERING_BGR,
    SUBPIXEL_RENDERING_VRGB,
    SUBPIXEL_RENDERING_VBGR,
  };

  // Antialiasing (grayscale if |subpixel_rendering| is SUBPIXEL_RENDERING_NONE
  // and RGBA otherwise).
  bool antialiasing;

  // Should subpixel positioning (i.e. fractional X positions for glyphs) be
  // used?
  bool subpixel_positioning;

  // Should FreeType's autohinter be used (as opposed to Freetype's bytecode
  // interpreter, which uses fonts' own hinting instructions)?
  bool autohinter;

  // Should embedded bitmaps in fonts should be used?
  bool use_bitmaps;

  // Hinting level.
  Hinting hinting;

  // Whether subpixel rendering should be used or not, and if so, the display's
  // subpixel order.
  SubpixelRendering subpixel_rendering;
};

// Returns the system's default parameters for font rendering.
GFX_EXPORT const FontRenderParams& GetDefaultFontRenderParams();

// Returns the system's default parameters for WebKit font rendering.
// TODO(derat): Rename to GetDefaultFontRenderParamsForWebContents().
GFX_EXPORT const FontRenderParams& GetDefaultWebKitFontRenderParams();

// Returns the appropriate parameters for rendering the font described by the
// passed-in-arguments, any of which may be NULL. If |family_out| is non-NULL,
// it will be updated to contain the recommended font family from |family_list|.
// |style| optionally points to a bit field of Font::FontStyle values.
GFX_EXPORT FontRenderParams GetCustomFontRenderParams(
    bool for_web_contents,
    const std::vector<std::string>* family_list,
    const int* pixel_size,
    const int* point_size,
    const int* style,
    std::string* family_out);

}  // namespace gfx

#endif  // UI_GFX_FONT_RENDER_PARAMS_H_
