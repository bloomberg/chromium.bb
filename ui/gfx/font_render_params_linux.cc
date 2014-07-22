// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/gfx/font.h"
#include "ui/gfx/linux_font_delegate.h"
#include "ui/gfx/switches.h"

#include <fontconfig/fontconfig.h>

namespace gfx {

namespace {

// Converts Fontconfig FC_HINT_STYLE to FontRenderParams::Hinting.
FontRenderParams::Hinting ConvertFontconfigHintStyle(int hint_style) {
  switch (hint_style) {
    case FC_HINT_SLIGHT: return FontRenderParams::HINTING_SLIGHT;
    case FC_HINT_MEDIUM: return FontRenderParams::HINTING_MEDIUM;
    case FC_HINT_FULL:   return FontRenderParams::HINTING_FULL;
    default:             return FontRenderParams::HINTING_NONE;
  }
}

// Converts Fontconfig FC_RGBA to FontRenderParams::SubpixelRendering.
FontRenderParams::SubpixelRendering ConvertFontconfigRgba(int rgba) {
  switch (rgba) {
    case FC_RGBA_RGB:  return FontRenderParams::SUBPIXEL_RENDERING_RGB;
    case FC_RGBA_BGR:  return FontRenderParams::SUBPIXEL_RENDERING_BGR;
    case FC_RGBA_VRGB: return FontRenderParams::SUBPIXEL_RENDERING_VRGB;
    case FC_RGBA_VBGR: return FontRenderParams::SUBPIXEL_RENDERING_VBGR;
    default:           return FontRenderParams::SUBPIXEL_RENDERING_NONE;
  }
}

// Queries Fontconfig for rendering settings and updates |params_out| and
// |family_out| (if non-NULL). Returns false on failure. See
// GetCustomFontRenderParams() for descriptions of arguments.
bool QueryFontconfig(const std::vector<std::string>* family_list,
                     const int* pixel_size,
                     const int* point_size,
                     const int* style,
                     FontRenderParams* params_out,
                     std::string* family_out) {
  FcPattern* pattern = FcPatternCreate();
  CHECK(pattern);

  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

  if (family_list) {
    for (std::vector<std::string>::const_iterator it = family_list->begin();
         it != family_list->end(); ++it) {
      FcPatternAddString(
          pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(it->c_str()));
    }
  }
  if (pixel_size)
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, *pixel_size);
  if (point_size)
    FcPatternAddInteger(pattern, FC_SIZE, *point_size);
  if (style) {
    FcPatternAddInteger(pattern, FC_SLANT,
        (*style & Font::ITALIC) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcPatternAddInteger(pattern, FC_WEIGHT,
        (*style & Font::BOLD) ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL);
  }

  FcConfigSubstitute(NULL, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(NULL, pattern, &result);
  FcPatternDestroy(pattern);
  if (!match)
    return false;

  if (family_out) {
    FcChar8* family = NULL;
    FcPatternGetString(match, FC_FAMILY, 0, &family);
    if (family)
      family_out->assign(reinterpret_cast<const char*>(family));
  }

  if (params_out) {
    FcBool fc_antialias = 0;
    if (FcPatternGetBool(match, FC_ANTIALIAS, 0, &fc_antialias) ==
        FcResultMatch) {
      params_out->antialiasing = fc_antialias;
    }

    FcBool fc_autohint = 0;
    if (FcPatternGetBool(match, FC_AUTOHINT, 0, &fc_autohint) ==
        FcResultMatch) {
      params_out->autohinter = fc_autohint;
    }

    FcBool fc_bitmap = 0;
    if (FcPatternGetBool(match, FC_EMBEDDED_BITMAP, 0, &fc_bitmap) ==
        FcResultMatch) {
      params_out->use_bitmaps = fc_bitmap;
    }

    FcBool fc_hinting = 0;
    if (FcPatternGetBool(match, FC_HINTING, 0, &fc_hinting) == FcResultMatch) {
      int fc_hint_style = FC_HINT_NONE;
      if (fc_hinting)
        FcPatternGetInteger(match, FC_HINT_STYLE, 0, &fc_hint_style);
      params_out->hinting = ConvertFontconfigHintStyle(fc_hint_style);
    }

    int fc_rgba = FC_RGBA_NONE;
    if (FcPatternGetInteger(match, FC_RGBA, 0, &fc_rgba) == FcResultMatch)
      params_out->subpixel_rendering = ConvertFontconfigRgba(fc_rgba);
  }

  FcPatternDestroy(match);
  return true;
}

// Returns the system's default settings.
FontRenderParams LoadDefaults(bool for_web_contents) {
  return GetCustomFontRenderParams(
      for_web_contents, NULL, NULL, NULL, NULL, NULL);
}

}  // namespace

const FontRenderParams& GetDefaultFontRenderParams() {
  static FontRenderParams default_params = LoadDefaults(false);
  return default_params;
}

const FontRenderParams& GetDefaultWebKitFontRenderParams() {
  static FontRenderParams default_params = LoadDefaults(true);
  return default_params;
}

FontRenderParams GetCustomFontRenderParams(
    bool for_web_contents,
    const std::vector<std::string>* family_list,
    const int* pixel_size,
    const int* point_size,
    const int* style,
    std::string* family_out) {
  if (family_out)
    family_out->clear();

  // Start with the delegate's settings, but let Fontconfig have the final say.
  FontRenderParams params;
  const LinuxFontDelegate* delegate = LinuxFontDelegate::instance();
  if (delegate)
    params = delegate->GetDefaultFontRenderParams();
  QueryFontconfig(
      family_list, pixel_size, point_size, style, &params, family_out);

  // Fontconfig doesn't support configuring subpixel positioning; check a flag.
  params.subpixel_positioning = CommandLine::ForCurrentProcess()->HasSwitch(
      for_web_contents ?
      switches::kEnableWebkitTextSubpixelPositioning :
      switches::kEnableBrowserTextSubpixelPositioning);

  // To enable subpixel positioning, we need to disable hinting.
  if (params.subpixel_positioning)
    params.hinting = FontRenderParams::HINTING_NONE;

  // Use the first family from the list if Fontconfig didn't suggest a family.
  if (family_out && family_out->empty() &&
      family_list && !family_list->empty())
    *family_out = (*family_list)[0];

  return params;
}

}  // namespace gfx
