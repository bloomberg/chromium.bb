/*
 * Copyright (C) 2006, 2007 Apple Computer, Inc.
 * Copyright (c) 2006, 2007, 2008, 2009, 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "platform/fonts/FontCache.h"

#include "SkFontMgr.h"
#include "SkTypeface_win.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/FontFaceCreationParams.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/harfbuzz/FontPlatformDataHarfbuzz.h"
#include "platform/fonts/win/FontFallbackWin.h"

namespace blink {

HashMap<String, RefPtr<SkTypeface> >* FontCache::s_sideloadedFonts = 0;

// static
void FontCache::addSideloadedFontForTesting(SkTypeface* typeface)
{
    if (!s_sideloadedFonts)
        s_sideloadedFonts = new HashMap<String, RefPtr<SkTypeface> >;
    SkString name;
    typeface->getFamilyName(&name);
    s_sideloadedFonts->set(name.c_str(), adoptRef(typeface));
}

FontCache::FontCache()
    : m_purgePreventCount(0)
{
    SkFontMgr* fontManager;

    if (s_useDirectWrite) {
        fontManager = SkFontMgr_New_DirectWrite(s_directWriteFactory);
        s_useSubpixelPositioning = RuntimeEnabledFeatures::subpixelFontScalingEnabled();
    } else {
        fontManager = SkFontMgr_New_GDI();
        // Subpixel text positioning is not supported by the GDI backend.
        s_useSubpixelPositioning = false;
    }

    ASSERT(fontManager);
    m_fontManager = adoptPtr(fontManager);
}


// Given the desired base font, this will create a SimpleFontData for a specific
// font that can be used to render the given range of characters.
PassRefPtr<SimpleFontData> FontCache::fallbackFontForCharacter(const FontDescription& fontDescription, UChar32 character, const SimpleFontData*)
{
    // First try the specified font with standard style & weight.
    if (fontDescription.style() == FontStyleItalic
        || fontDescription.weight() >= FontWeightBold) {
        RefPtr<SimpleFontData> fontData = fallbackOnStandardFontStyle(
            fontDescription, character);
        if (fontData)
            return fontData;
    }

    // FIXME: Consider passing fontDescription.dominantScript()
    // to GetFallbackFamily here.
    UScriptCode script;
    const wchar_t* family = getFallbackFamily(character,
        fontDescription.genericFamily(),
        &script,
        m_fontManager.get());
    FontPlatformData* data = 0;
    if (family) {
        FontFaceCreationParams createByFamily(AtomicString(family, wcslen(family)));
        data = getFontPlatformData(fontDescription, createByFamily);
    }

    // Last resort font list : PanUnicode. CJK fonts have a pretty
    // large repertoire. Eventually, we need to scan all the fonts
    // on the system to have a Firefox-like coverage.
    // Make sure that all of them are lowercased.
    const static wchar_t* const cjkFonts[] = {
        L"arial unicode ms",
        L"ms pgothic",
        L"simsun",
        L"gulim",
        L"pmingliu",
        L"wenquanyi zen hei", // Partial CJK Ext. A coverage but more widely known to Chinese users.
        L"ar pl shanheisun uni",
        L"ar pl zenkai uni",
        L"han nom a", // Complete CJK Ext. A coverage.
        L"code2000" // Complete CJK Ext. A coverage.
        // CJK Ext. B fonts are not listed here because it's of no use
        // with our current non-BMP character handling because we use
        // Uniscribe for it and that code path does not go through here.
    };

    const static wchar_t* const commonFonts[] = {
        L"tahoma",
        L"arial unicode ms",
        L"lucida sans unicode",
        L"microsoft sans serif",
        L"palatino linotype",
        // Six fonts below (and code2000 at the end) are not from MS, but
        // once installed, cover a very wide range of characters.
        L"dejavu serif",
        L"dejavu sasns",
        L"freeserif",
        L"freesans",
        L"gentium",
        L"gentiumalt",
        L"ms pgothic",
        L"simsun",
        L"gulim",
        L"pmingliu",
        L"code2000"
    };

    const wchar_t* const* panUniFonts = 0;
    int numFonts = 0;
    if (script == USCRIPT_HAN) {
        panUniFonts = cjkFonts;
        numFonts = WTF_ARRAY_LENGTH(cjkFonts);
    } else {
        panUniFonts = commonFonts;
        numFonts = WTF_ARRAY_LENGTH(commonFonts);
    }
    // Font returned from getFallbackFamily may not cover |character|
    // because it's based on script to font mapping. This problem is
    // critical enough for non-Latin scripts (especially Han) to
    // warrant an additional (real coverage) check with fontCotainsCharacter.
    int i;
    for (i = 0; (!data || !data->fontContainsCharacter(character)) && i < numFonts; ++i) {
        family = panUniFonts[i];
        FontFaceCreationParams createByFamily(AtomicString(family, wcslen(family)));
        data = getFontPlatformData(fontDescription, createByFamily);
    }

    // When i-th font (0-base) in |panUniFonts| contains a character and
    // we get out of the loop, |i| will be |i + 1|. That is, if only the
    // last font in the array covers the character, |i| will be numFonts.
    // So, we have to use '<=" rather than '<' to see if we found a font
    // covering the character.
    if (i <= numFonts)
        return fontDataFromFontPlatformData(data, DoNotRetain);

    return nullptr;
}

static inline bool equalIgnoringCase(const AtomicString& a, const SkString& b)
{
    return equalIgnoringCase(a, AtomicString::fromUTF8(b.c_str()));
}

static bool typefacesMatchesFamily(const SkTypeface* tf, const AtomicString& family)
{
    SkTypeface::LocalizedStrings* actualFamilies = tf->createFamilyNameIterator();
    bool matchesRequestedFamily = false;
    SkTypeface::LocalizedString actualFamily;

    while (actualFamilies->next(&actualFamily)) {
        if (equalIgnoringCase(family, actualFamily.fString)) {
            matchesRequestedFamily = true;
            break;
        }
    }
    actualFamilies->unref();

    // getFamilyName may return a name not returned by the createFamilyNameIterator.
    // Specifically in cases where Windows substitutes the font based on the
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\FontSubstitutes registry entries.
    if (!matchesRequestedFamily) {
        SkString familyName;
        tf->getFamilyName(&familyName);
        if (equalIgnoringCase(family, familyName))
            matchesRequestedFamily = true;
    }

    return matchesRequestedFamily;
}


static bool typefacesHasVariantSuffix(const AtomicString& family,
    AtomicString& adjustedName, FontWeight& variantWeight)
{
    struct FamilyVariantSuffix {
        const wchar_t* suffix;
        size_t length;
        FontWeight weight;
    };
    // Mapping from suffix to weight from the DirectWrite documentation.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/dd368082(v=vs.85).aspx
    const static FamilyVariantSuffix variantForSuffix[] = {
        { L" thin", 5,  FontWeight100 },
        { L" extralight", 11,  FontWeight200 },
        { L" ultralight", 11,  FontWeight200 },
        { L" light", 6,  FontWeight300 },
        { L" medium", 7,  FontWeight500 },
        { L" demibold", 9,  FontWeight600 },
        { L" semibold", 9,  FontWeight600 },
        { L" extrabold", 10,  FontWeight800 },
        { L" ultrabold", 10,  FontWeight800 },
        { L" black", 6,  FontWeight900 },
        { L" heavy", 6,  FontWeight900 }
    };
    size_t numVariants = WTF_ARRAY_LENGTH(variantForSuffix);
    bool caseSensitive = false;
    for (size_t i = 0; i < numVariants; i++) {
        const FamilyVariantSuffix& entry = variantForSuffix[i];
        if (family.endsWith(entry.suffix, caseSensitive)) {
            String familyName = family.string();
            familyName.truncate(family.length() - entry.length);
            adjustedName = AtomicString(familyName);
            variantWeight = entry.weight;
            return true;
        }
    }

    return false;
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const FontFaceCreationParams& creationParams, float fontSize)
{
    ASSERT(creationParams.creationType() == CreateFontByFamily);

    CString name;
    RefPtr<SkTypeface> tf = createTypeface(fontDescription, creationParams, name);
    // Windows will always give us a valid pointer here, even if the face name
    // is non-existent. We have to double-check and see if the family name was
    // really used.
    if (!tf || !typefacesMatchesFamily(tf.get(), creationParams.family())) {
        AtomicString adjustedName;
        FontWeight variantWeight;
        if (typefacesHasVariantSuffix(creationParams.family(), adjustedName,
            variantWeight)) {
            FontFaceCreationParams adjustedParams(adjustedName);
            FontDescription adjustedFontDescription = fontDescription;
            adjustedFontDescription.setWeight(variantWeight);
            tf = createTypeface(adjustedFontDescription, adjustedParams, name);
            if (!tf || !typefacesMatchesFamily(tf.get(), adjustedName))
                return 0;
        } else {
            return 0;
        }
    }

    FontPlatformData* result = new FontPlatformData(tf,
        name.data(),
        fontSize,
        fontDescription.weight() >= FontWeight600 && !tf->isBold() || fontDescription.isSyntheticBold(),
        fontDescription.style() == FontStyleItalic && !tf->isItalic() || fontDescription.isSyntheticItalic(),
        fontDescription.orientation(),
        s_useSubpixelPositioning);

    struct FamilyMinSize {
        const wchar_t* family;
        unsigned minSize;
    };
    const static FamilyMinSize minAntiAliasSizeForFont[] = {
        { L"simsun", 11 },
        { L"dotum", 12 },
        { L"gulim", 12 },
        { L"pmingliu", 11 }
    };
    size_t numFonts = WTF_ARRAY_LENGTH(minAntiAliasSizeForFont);
    for (size_t i = 0; i < numFonts; i++) {
        FamilyMinSize entry = minAntiAliasSizeForFont[i];
        if (typefacesMatchesFamily(tf.get(), entry.family)) {
            result->setMinSizeForAntiAlias(entry.minSize);
            break;
        }
    }

    // List of fonts that look bad with subpixel text rendering at smaller font
    // sizes. This includes all fonts in the Microsoft Core fonts for the Web
    // collection.
    const static wchar_t* noSubpixelForSmallSizeFont[] = {
        L"andale mono",
        L"arial",
        L"comic sans",
        L"courier new",
        L"georgia",
        L"impact",
        L"lucida console",
        L"tahoma",
        L"times new roman",
        L"trebuchet ms",
        L"verdana",
        L"webdings"
    };
    const static float minSizeForSubpixelForFont = 16.0f;
    numFonts = WTF_ARRAY_LENGTH(noSubpixelForSmallSizeFont);
    for (size_t i = 0; i < numFonts; i++) {
        const wchar_t* family = noSubpixelForSmallSizeFont[i];
        if (typefacesMatchesFamily(tf.get(), family)) {
            result->setMinSizeForSubpixel(minSizeForSubpixelForFont);
            break;
        }
    }

    return result;
}

} // namespace blink
