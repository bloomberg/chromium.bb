/*
 * Copyright (c) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include "SkFontMgr.h"
#include "SkStream.h"
#include "SkTypeface.h"
#include "platform/fonts/AlternateFontFamily.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFaceCreationParams.h"
#include "platform/fonts/SimpleFontData.h"
#include "public/platform/Platform.h"
#include "public/platform/linux/WebSandboxSupport.h"
#include "wtf/Assertions.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/CString.h"
#include <unicode/locid.h>

// Unfortunately, these chosen font names require a bit of experimentation and
// researching on the respective platforms as to what works best and is widely
// available. They may require further manual tuning. Mozilla's choices in the
// gfxPlatform*::GetCommonFallbackFonts methods collects some of the outcome of
// this experimentation. On Android, the available fonts in
// /system/etc/fonts.xml give a clearer picture of which fonts are available and
// can be widely used successfully. On Linux, updating and improving this list
// requires finding out widely used distribution fonts, for example on current
// Ubuntu versions.
#if OS(WIN)
const char* kColorEmojiFonts[] = { "Segoe UI Emoji", "Segoe UI Symbol" };
const char* kTextEmojiFonts[] = { "Segoe UI Symbol", "Code2000", "Lucida Sans Unicode" };
const char* kSymbolsFonts[] = { "Segoe UI Symbol", "Code2000", "Lucida Sans Unicode" };
const char* kMathFonts[] = { "Cambria Math", "Segoe UI Symbol", "Code2000" };
#elif OS(ANDROID)
// Due to crbug.com/322658 we cannot properly specify Android font family names here,
// The way Skia's SkFontMgr_android gives them canonical names, we can however
// reference the Noto Color Emoji font under "56##fallback" on Android 6.0.1
const char* kColorEmojiFonts[] = { "56##fallback", "Noto Color Emoji", "Android Emoji" };
const char* kTextEmojiFonts[] = { "Droid Sans Fallback" };
const char* kSymbolsFonts[] = { "Droid Sans Fallback" };
const char* kMathFonts[] = { "Droid Sans Fallback" };
#elif OS(LINUX)
const char* kColorEmojiFonts[] = { "Noto Color Emoji", "Noto Sans Symbols", "Symbola", "DejaVu Sans" };
const char* kTextEmojiFonts[] = { "Noto Sans Symbols", "Symbola", "Droid Sans Fallback", "DejaVu Sans" };
const char* kSymbolsFonts[] = { "FreeSerif", "FreeMono", "Droid Sans Fallback", "DejaVu Sans" };
const char* kMathFonts[] = { "FreeSerif", "FreeMono", "Droid Sans Fallback", "DejaVu Sans" };
#endif

#if !OS(WIN) && !OS(ANDROID)
#include "SkFontConfigInterface.h"

static PassRefPtr<SkTypeface> typefaceForFontconfigInterfaceIdAndTtcIndex(int fontconfigInterfaceId, int ttcIndex)
{
    SkAutoTUnref<SkFontConfigInterface> fci(SkFontConfigInterface::RefGlobal());
    SkFontConfigInterface::FontIdentity fontIdentity;
    fontIdentity.fID = fontconfigInterfaceId;
    fontIdentity.fTTCIndex = ttcIndex;
    return adoptRef(fci->createTypeface(fontIdentity));
}
#endif

namespace blink {

void FontCache::platformInit()
{
}

PassRefPtr<SimpleFontData> FontCache::fallbackOnStandardFontStyle(
    const FontDescription& fontDescription, UChar32 character)
{
    FontDescription substituteDescription(fontDescription);
    substituteDescription.setStyle(FontStyleNormal);
    substituteDescription.setWeight(FontWeightNormal);

    FontFaceCreationParams creationParams(substituteDescription.family().family());
    FontPlatformData* substitutePlatformData = getFontPlatformData(substituteDescription, creationParams);
    if (substitutePlatformData && substitutePlatformData->fontContainsCharacter(character)) {
        FontPlatformData platformData = FontPlatformData(*substitutePlatformData);
        platformData.setSyntheticBold(fontDescription.weight() >= FontWeight600);
        platformData.setSyntheticItalic(fontDescription.style() == FontStyleItalic || fontDescription.style() == FontStyleOblique);
        return fontDataFromFontPlatformData(&platformData, DoNotRetain);
    }

    return nullptr;
}

#if !OS(WIN) && !OS(ANDROID)
PassRefPtr<SimpleFontData> FontCache::fallbackFontForCharacter(const FontDescription& fontDescription, UChar32 c, const SimpleFontData*)
{
    // First try the specified font with standard style & weight.
    if (fontDescription.style() == FontStyleItalic
        || fontDescription.weight() >= FontWeight600) {
        RefPtr<SimpleFontData> fontData = fallbackOnStandardFontStyle(
            fontDescription, c);
        if (fontData)
            return fontData;
    }

    FontCache::PlatformFallbackFont fallbackFont;
    FontCache::getFontForCharacter(c, fontDescription.locale().ascii().data(), &fallbackFont);
    if (fallbackFont.name.isEmpty())
        return nullptr;

    FontFaceCreationParams creationParams;
    creationParams = FontFaceCreationParams(fallbackFont.filename, fallbackFont.fontconfigInterfaceId, fallbackFont.ttcIndex);

    // Changes weight and/or italic of given FontDescription depends on
    // the result of fontconfig so that keeping the correct font mapping
    // of the given character. See http://crbug.com/32109 for details.
    bool shouldSetSyntheticBold = false;
    bool shouldSetSyntheticItalic = false;
    FontDescription description(fontDescription);
    if (fallbackFont.isBold && description.weight() < FontWeightBold)
        description.setWeight(FontWeightBold);
    if (!fallbackFont.isBold && description.weight() >= FontWeightBold) {
        shouldSetSyntheticBold = true;
        description.setWeight(FontWeightNormal);
    }
    if (fallbackFont.isItalic && description.style() == FontStyleNormal)
        description.setStyle(FontStyleItalic);
    if (!fallbackFont.isItalic && (description.style() == FontStyleItalic || description.style() == FontStyleOblique)) {
        shouldSetSyntheticItalic = true;
        description.setStyle(FontStyleNormal);
    }

    FontPlatformData* substitutePlatformData = getFontPlatformData(description, creationParams);
    if (!substitutePlatformData)
        return nullptr;
    FontPlatformData platformData = FontPlatformData(*substitutePlatformData);
    platformData.setSyntheticBold(shouldSetSyntheticBold);
    platformData.setSyntheticItalic(shouldSetSyntheticItalic);
    return fontDataFromFontPlatformData(&platformData, DoNotRetain);
}

#endif // !OS(WIN) && !OS(ANDROID)

PassRefPtr<SimpleFontData> FontCache::getLastResortFallbackFont(const FontDescription& description, ShouldRetain shouldRetain)
{
    const FontFaceCreationParams fallbackCreationParams(getFallbackFontFamily(description));
    const FontPlatformData* fontPlatformData = getFontPlatformData(description, fallbackCreationParams);

    // We should at least have Sans or Arial which is the last resort fallback of SkFontHost ports.
    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const FontFaceCreationParams, sansCreationParams, (AtomicString("Sans", AtomicString::ConstructFromLiteral)));
        fontPlatformData = getFontPlatformData(description, sansCreationParams);
    }
    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const FontFaceCreationParams, arialCreationParams, (AtomicString("Arial", AtomicString::ConstructFromLiteral)));
        fontPlatformData = getFontPlatformData(description, arialCreationParams);
    }
#if OS(WIN)
    // Try some more Windows-specific fallbacks.
    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const FontFaceCreationParams, msuigothicCreationParams, (AtomicString("MS UI Gothic", AtomicString::ConstructFromLiteral)));
        fontPlatformData = getFontPlatformData(description, msuigothicCreationParams);
    }
    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const FontFaceCreationParams, mssansserifCreationParams, (AtomicString("Microsoft Sans Serif", AtomicString::ConstructFromLiteral)));
        fontPlatformData = getFontPlatformData(description, mssansserifCreationParams);
    }
#endif

    ASSERT(fontPlatformData);
    return fontDataFromFontPlatformData(fontPlatformData, shouldRetain);
}

const Vector<AtomicString> FontCache::platformFontListForFallbackPriority(FontFallbackPriority fallbackPriority) const
{
    Vector<AtomicString> returnVector;
    switch (fallbackPriority) {
    case FontFallbackPriority::EmojiEmoji:
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(kColorEmojiFonts); ++i)
            returnVector.append(kColorEmojiFonts[i]);
        break;
    case FontFallbackPriority::EmojiText:
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(kTextEmojiFonts); ++i)
            returnVector.append(kTextEmojiFonts[i]);
        break;
    case FontFallbackPriority::Math:
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(kMathFonts); ++i)
            returnVector.append(kMathFonts[i]);
        break;
    case FontFallbackPriority::Symbols:
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(kSymbolsFonts); ++i)
            returnVector.append(kSymbolsFonts[i]);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    return returnVector;
}

#if OS(WIN)
static inline SkFontStyle fontStyle(const FontDescription& fontDescription)
{
    int width = static_cast<int>(fontDescription.stretch());
    int weight = (fontDescription.weight() - FontWeight100 + 1) * 100;
    SkFontStyle::Slant slant = fontDescription.style() == FontStyleItalic
        ? SkFontStyle::kItalic_Slant
        : SkFontStyle::kUpright_Slant;
    return SkFontStyle(weight, width, slant);
}

static_assert(static_cast<int>(FontStretchUltraCondensed) == static_cast<int>(SkFontStyle::kUltraCondensed_Width),
    "FontStretchUltraCondensed should map to kUltraCondensed_Width");
static_assert(static_cast<int>(FontStretchNormal) == static_cast<int>(SkFontStyle::kNormal_Width),
    "FontStretchNormal should map to kNormal_Width");
static_assert(static_cast<int>(FontStretchUltraExpanded) == static_cast<int>(SkFontStyle::kUltaExpanded_Width),
    "FontStretchUltraExpanded should map to kUltaExpanded_Width");
#endif

PassRefPtr<SkTypeface> FontCache::createTypeface(const FontDescription& fontDescription, const FontFaceCreationParams& creationParams, CString& name)
{
#if !OS(WIN) && !OS(ANDROID)
    if (creationParams.creationType() == CreateFontByFciIdAndTtcIndex) {
        if (Platform::current()->sandboxSupport())
            return typefaceForFontconfigInterfaceIdAndTtcIndex(creationParams.fontconfigInterfaceId(), creationParams.ttcIndex());
        return adoptRef(SkTypeface::CreateFromFile(creationParams.filename().data(), creationParams.ttcIndex()));
    }
#endif

    AtomicString family = creationParams.family();
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (!family.length() || family.startsWith("-webkit-")) {
        name = getFallbackFontFamily(fontDescription).string().utf8();
    } else {
        // convert the name to utf8
        name = family.utf8();
    }

    int style = SkTypeface::kNormal;
    if (fontDescription.weight() >= FontWeight600)
        style |= SkTypeface::kBold;
    if (fontDescription.style())
        style |= SkTypeface::kItalic;

#if OS(WIN)
    if (s_sideloadedFonts) {
        HashMap<String, RefPtr<SkTypeface>>::iterator sideloadedFont =
            s_sideloadedFonts->find(name.data());
        if (sideloadedFont != s_sideloadedFonts->end())
            return sideloadedFont->value;
    }

    if (m_fontManager) {
        return adoptRef(useDirectWrite()
            ? m_fontManager->matchFamilyStyle(name.data(), fontStyle(fontDescription))
            : m_fontManager->legacyCreateTypeface(name.data(), style)
            );
    }
#endif

    // FIXME: Use m_fontManager, SkFontStyle and matchFamilyStyle instead of
    // CreateFromName on all platforms.
    return adoptRef(SkTypeface::CreateFromName(name.data(), static_cast<SkTypeface::Style>(style)));
}

#if !OS(WIN)
PassOwnPtr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription,
    const FontFaceCreationParams& creationParams, float fontSize)
{
    CString name;
    RefPtr<SkTypeface> tf(createTypeface(fontDescription, creationParams, name));
    if (!tf)
        return nullptr;

    return adoptPtr(new FontPlatformData(tf,
        name.data(),
        fontSize,
        (fontDescription.weight() >= FontWeight600 && !tf->isBold()) || fontDescription.isSyntheticBold(),
        ((fontDescription.style() == FontStyleItalic || fontDescription.style() == FontStyleOblique) && !tf->isItalic()) || fontDescription.isSyntheticItalic(),
        fontDescription.orientation(),
        fontDescription.useSubpixelPositioning()));
}
#endif // !OS(WIN)

} // namespace blink
