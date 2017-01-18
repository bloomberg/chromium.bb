// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/linux/FontRenderStyle.h"

#include "platform/LayoutTestSupport.h"
#include "platform/fonts/FontDescription.h"
#include "public/platform/Platform.h"
#include "public/platform/linux/WebFontRenderStyle.h"
#include "public/platform/linux/WebSandboxSupport.h"

namespace blink {

namespace {

SkPaint::Hinting skiaHinting = SkPaint::kNormal_Hinting;
bool useSkiaAutoHint = true;
bool useSkiaBitmaps = true;
bool useSkiaAntiAlias = true;
bool useSkiaSubpixelRendering = false;

}  // namespace

// static
void FontRenderStyle::setHinting(SkPaint::Hinting hinting) {
  skiaHinting = hinting;
}

// static
void FontRenderStyle::setAutoHint(bool useAutoHint) {
  useSkiaAutoHint = useAutoHint;
}

// static
void FontRenderStyle::setUseBitmaps(bool useBitmaps) {
  useSkiaBitmaps = useBitmaps;
}

// static
void FontRenderStyle::setAntiAlias(bool useAntiAlias) {
  useSkiaAntiAlias = useAntiAlias;
}

// static
void FontRenderStyle::setSubpixelRendering(bool useSubpixelRendering) {
  useSkiaSubpixelRendering = useSubpixelRendering;
}

// static
FontRenderStyle FontRenderStyle::querySystem(const CString& family,
                                             float textSize,
                                             SkFontStyle fontStyle) {
  WebFontRenderStyle style;
#if OS(ANDROID)
  style.setDefaults();
#else
  // If the font name is missing (i.e. probably a web font) or the sandbox is
  // disabled, use the system defaults.
  if (!family.length() || !Platform::current()->sandboxSupport()) {
    style.setDefaults();
  } else {
    bool isBold = fontStyle.weight() >= SkFontStyle::kSemiBold_Weight;
    bool isItalic = fontStyle.slant() != SkFontStyle::kUpright_Slant;
    const int sizeAndStyle =
        (((int)textSize) << 2) | (((int)isBold) << 1) | (((int)isItalic) << 0);
    Platform::current()->sandboxSupport()->getWebFontRenderStyleForStrike(
        family.data(), sizeAndStyle, &style);
  }
#endif

  FontRenderStyle result;
  style.toFontRenderStyle(&result);

  // Fix FontRenderStyle::NoPreference to actual styles.
  if (result.useAntiAlias == FontRenderStyle::NoPreference)
    result.useAntiAlias = useSkiaAntiAlias;

  if (!result.useHinting)
    result.hintStyle = SkPaint::kNo_Hinting;
  else if (result.useHinting == FontRenderStyle::NoPreference)
    result.hintStyle = skiaHinting;

  if (result.useBitmaps == FontRenderStyle::NoPreference)
    result.useBitmaps = useSkiaBitmaps;
  if (result.useAutoHint == FontRenderStyle::NoPreference)
    result.useAutoHint = useSkiaAutoHint;
  if (result.useAntiAlias == FontRenderStyle::NoPreference)
    result.useAntiAlias = useSkiaAntiAlias;
  if (result.useSubpixelRendering == FontRenderStyle::NoPreference)
    result.useSubpixelRendering = useSkiaSubpixelRendering;

  // TestRunner specifically toggles the subpixel positioning flag.
  if (result.useSubpixelPositioning == FontRenderStyle::NoPreference ||
      LayoutTestSupport::isRunningLayoutTest())
    result.useSubpixelPositioning = FontDescription::subpixelPositioning();

  return result;
}

void FontRenderStyle::applyToPaint(SkPaint& paint,
                                   float deviceScaleFactor) const {
  paint.setAntiAlias(useAntiAlias);
  paint.setHinting(static_cast<SkPaint::Hinting>(hintStyle));
  paint.setEmbeddedBitmapText(useBitmaps);
  paint.setAutohinted(useAutoHint);
  if (useAntiAlias)
    paint.setLCDRenderText(useSubpixelRendering);

  // Do not enable subpixel text on low-dpi if full hinting is requested.
  bool useSubpixelText = (paint.getHinting() != SkPaint::kFull_Hinting ||
                          deviceScaleFactor > 1.0f);

  // TestRunner specifically toggles the subpixel positioning flag.
  if (useSubpixelText && !LayoutTestSupport::isRunningLayoutTest())
    paint.setSubpixelText(true);
  else
    paint.setSubpixelText(useSubpixelPositioning);
}

}  // namespace blink
