// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/linux/FontRenderStyle.h"

#include "build/build_config.h"
#include "platform/LayoutTestSupport.h"
#include "platform/fonts/FontDescription.h"
#include "public/platform/Platform.h"
#include "public/platform/linux/WebFontRenderStyle.h"
#include "public/platform/linux/WebSandboxSupport.h"

namespace blink {

namespace {

SkPaint::Hinting g_skia_hinting = SkPaint::kNormal_Hinting;
bool g_use_skia_auto_hint = true;
bool g_use_skia_bitmaps = true;
bool g_use_skia_anti_alias = true;
bool g_use_skia_subpixel_rendering = false;

}  // namespace

// static
void FontRenderStyle::SetHinting(SkPaint::Hinting hinting) {
  g_skia_hinting = hinting;
}

// static
void FontRenderStyle::SetAutoHint(bool use_auto_hint) {
  g_use_skia_auto_hint = use_auto_hint;
}

// static
void FontRenderStyle::SetUseBitmaps(bool use_bitmaps) {
  g_use_skia_bitmaps = use_bitmaps;
}

// static
void FontRenderStyle::SetAntiAlias(bool use_anti_alias) {
  g_use_skia_anti_alias = use_anti_alias;
}

// static
void FontRenderStyle::SetSubpixelRendering(bool use_subpixel_rendering) {
  g_use_skia_subpixel_rendering = use_subpixel_rendering;
}

// static
FontRenderStyle FontRenderStyle::QuerySystem(const CString& family,
                                             float text_size,
                                             SkFontStyle font_style) {
  WebFontRenderStyle style;
#if defined(OS_ANDROID)
  style.SetDefaults();
#else
  // If the font name is missing (i.e. probably a web font) or the sandbox is
  // disabled, use the system defaults.
  if (!family.length() || !Platform::Current()->GetSandboxSupport()) {
    style.SetDefaults();
  } else {
    bool is_bold = font_style.weight() >= SkFontStyle::kSemiBold_Weight;
    bool is_italic = font_style.slant() != SkFontStyle::kUpright_Slant;
    const int size_and_style = (((int)text_size) << 2) | (((int)is_bold) << 1) |
                               (((int)is_italic) << 0);
    Platform::Current()->GetSandboxSupport()->GetWebFontRenderStyleForStrike(
        family.data(), size_and_style, &style);
  }
#endif

  FontRenderStyle result;
  style.ToFontRenderStyle(&result);

  // Fix FontRenderStyle::NoPreference to actual styles.
  if (result.use_anti_alias == FontRenderStyle::kNoPreference)
    result.use_anti_alias = g_use_skia_anti_alias;

  if (!result.use_hinting)
    result.hint_style = SkPaint::kNo_Hinting;
  else if (result.use_hinting == FontRenderStyle::kNoPreference)
    result.hint_style = g_skia_hinting;

  if (result.use_bitmaps == FontRenderStyle::kNoPreference)
    result.use_bitmaps = g_use_skia_bitmaps;
  if (result.use_auto_hint == FontRenderStyle::kNoPreference)
    result.use_auto_hint = g_use_skia_auto_hint;
  if (result.use_anti_alias == FontRenderStyle::kNoPreference)
    result.use_anti_alias = g_use_skia_anti_alias;
  if (result.use_subpixel_rendering == FontRenderStyle::kNoPreference)
    result.use_subpixel_rendering = g_use_skia_subpixel_rendering;

  // TestRunner specifically toggles the subpixel positioning flag.
  if (result.use_subpixel_positioning == FontRenderStyle::kNoPreference ||
      LayoutTestSupport::IsRunningLayoutTest())
    result.use_subpixel_positioning = FontDescription::SubpixelPositioning();

  return result;
}

void FontRenderStyle::ApplyToPaint(SkPaint& paint,
                                   float device_scale_factor) const {
  paint.setAntiAlias(use_anti_alias);
  paint.setHinting(static_cast<SkPaint::Hinting>(hint_style));
  paint.setEmbeddedBitmapText(use_bitmaps);
  paint.setAutohinted(use_auto_hint);
  if (use_anti_alias)
    paint.setLCDRenderText(use_subpixel_rendering);

  // Do not enable subpixel text on low-dpi if full hinting is requested.
  bool use_subpixel_text = (paint.getHinting() != SkPaint::kFull_Hinting ||
                            device_scale_factor > 1.0f);

  // TestRunner specifically toggles the subpixel positioning flag.
  if (use_subpixel_text && !LayoutTestSupport::IsRunningLayoutTest())
    paint.setSubpixelText(true);
  else
    paint.setSubpixelText(use_subpixel_positioning);
}

}  // namespace blink
