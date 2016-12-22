/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "platform/fonts/FontPlatformData.h"

#import "platform/LayoutTestSupport.h"
#import "platform/fonts/Font.h"
#import "platform/fonts/opentype/FontSettings.h"
#import "platform/fonts/shaping/HarfBuzzFace.h"
#import "platform/graphics/skia/SkiaUtils.h"
#import "public/platform/Platform.h"
#import "public/platform/mac/WebSandboxSupport.h"
#import "third_party/skia/include/ports/SkTypeface_mac.h"
#import "wtf/RetainPtr.h"
#import "wtf/text/WTFString.h"
#import <AppKit/NSFont.h>
#import <AvailabilityMacros.h>

namespace blink {

static bool canLoadInProcess(NSFont* nsFont) {
  RetainPtr<CGFontRef> cgFont(AdoptCF,
                              CTFontCopyGraphicsFont(toCTFontRef(nsFont), 0));
  // Toll-free bridged types CFStringRef and NSString*.
  RetainPtr<NSString> fontName(
      AdoptNS, const_cast<NSString*>(reinterpret_cast<const NSString*>(
                   CGFontCopyPostScriptName(cgFont.get()))));
  return ![fontName.get() isEqualToString:@"LastResort"];
}

static CTFontDescriptorRef cascadeToLastResortFontDescriptor() {
  static CTFontDescriptorRef descriptor;
  if (descriptor)
    return descriptor;

  RetainPtr<CTFontDescriptorRef> lastResort(
      AdoptCF, CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));
  const void* descriptors[] = {lastResort.get()};
  RetainPtr<CFArrayRef> valuesArray(
      AdoptCF,
      CFArrayCreate(kCFAllocatorDefault, descriptors,
                    WTF_ARRAY_LENGTH(descriptors), &kCFTypeArrayCallBacks));

  const void* keys[] = {kCTFontCascadeListAttribute};
  const void* values[] = {valuesArray.get()};
  RetainPtr<CFDictionaryRef> attributes(
      AdoptCF,
      CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                         WTF_ARRAY_LENGTH(keys), &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));

  descriptor = CTFontDescriptorCreateWithAttributes(attributes.get());

  return descriptor;
}

static sk_sp<SkTypeface> loadFromBrowserProcess(NSFont* nsFont,
                                                float textSize) {
  // Send cross-process request to load font.
  WebSandboxSupport* sandboxSupport = Platform::current()->sandboxSupport();
  if (!sandboxSupport) {
    // This function should only be called in response to an error loading a
    // font due to being blocked by the sandbox.
    // This by definition shouldn't happen if there is no sandbox support.
    ASSERT_NOT_REACHED();
    return nullptr;
  }

  CGFontRef loadedCgFont;
  uint32_t fontID;
  if (!sandboxSupport->loadFont(nsFont, &loadedCgFont, &fontID)) {
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Loading user font \"" << [[nsFont familyName] UTF8String]
        << "\" from non system location failed. Corrupt or missing font file?";
    return nullptr;
  }
  RetainPtr<CGFontRef> cgFont(AdoptCF, loadedCgFont);
  RetainPtr<CTFontRef> ctFont(
      AdoptCF,
      CTFontCreateWithGraphicsFont(cgFont.get(), textSize, 0,
                                   cascadeToLastResortFontDescriptor()));
  sk_sp<SkTypeface> returnFont(
      SkCreateTypefaceFromCTFont(ctFont.get(), cgFont.get()));

  if (!returnFont.get())
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Instantiating SkTypeface from user font failed for font family \""
        << [[nsFont familyName] UTF8String] << "\".";
  return returnFont;
}

void FontPlatformData::setupPaint(SkPaint* paint,
                                  float,
                                  const Font* font) const {
  bool shouldSmoothFonts = true;
  bool shouldAntialias = true;

  if (font) {
    switch (font->getFontDescription().fontSmoothing()) {
      case Antialiased:
        shouldSmoothFonts = false;
        break;
      case SubpixelAntialiased:
        break;
      case NoSmoothing:
        shouldAntialias = false;
        shouldSmoothFonts = false;
        break;
      case AutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default
        // settings.
        break;
    }
  }

  if (LayoutTestSupport::isRunningLayoutTest()) {
    shouldSmoothFonts = false;
    shouldAntialias = shouldAntialias &&
                      LayoutTestSupport::isFontAntialiasingEnabledForTest();
  }

  paint->setAntiAlias(shouldAntialias);
  paint->setEmbeddedBitmapText(false);
  const float ts = m_textSize >= 0 ? m_textSize : 12;
  paint->setTextSize(SkFloatToScalar(ts));
  paint->setTypeface(m_typeface);
  paint->setFakeBoldText(m_syntheticBold);
  paint->setTextSkewX(m_syntheticItalic ? -SK_Scalar1 / 4 : 0);
  paint->setLCDRenderText(shouldSmoothFonts);
  paint->setSubpixelText(true);

  // When rendering using CoreGraphics, disable hinting when
  // webkit-font-smoothing:antialiased or text-rendering:geometricPrecision is
  // used.  See crbug.com/152304
  if (font &&
      (font->getFontDescription().fontSmoothing() == Antialiased ||
       font->getFontDescription().textRendering() == GeometricPrecision))
    paint->setHinting(SkPaint::kNo_Hinting);
}

FontPlatformData::FontPlatformData(NSFont* nsFont,
                                   float size,
                                   bool syntheticBold,
                                   bool syntheticItalic,
                                   FontOrientation orientation,
                                   FontVariationSettings* variationSettings)
    : m_textSize(size),
      m_syntheticBold(syntheticBold),
      m_syntheticItalic(syntheticItalic),
      m_orientation(orientation),
      m_isHashTableDeletedValue(false) {
  DCHECK(nsFont);
  if (canLoadInProcess(nsFont)) {
    m_typeface.reset(SkCreateTypefaceFromCTFont(toCTFontRef(nsFont)));
  } else {
    // In process loading fails for cases where third party font manager
    // software registers fonts in non system locations such as /Library/Fonts
    // and ~/Library Fonts, see crbug.com/72727 or crbug.com/108645.
    m_typeface = loadFromBrowserProcess(nsFont, size);
  }

  if (variationSettings && variationSettings->size() < UINT16_MAX) {
    SkFontMgr::FontParameters::Axis axes[variationSettings->size()];
    for (size_t i = 0; i < variationSettings->size(); ++i) {
      AtomicString featureTag = variationSettings->at(i).tag();
      axes[i] = {atomicStringToFourByteTag(featureTag),
                 SkFloatToScalar(variationSettings->at(i).value())};
    }
    sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
    // TODO crbug.com/670246: Refactor this to a future Skia API that acccepts
    // axis parameters on system fonts directly.
    m_typeface = sk_sp<SkTypeface>(fm->createFromStream(
        m_typeface->openStream(nullptr)->duplicate(),
        SkFontMgr::FontParameters().setAxes(axes, variationSettings->size())));
  }
}

}  // namespace blink
