/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebFontInfo.h"

#include "WebFontRenderStyle.h"
#include "public/platform/linux/WebFontFamily.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicStringHash.h"
#include <fontconfig/fontconfig.h>
#include <string.h>
#include <unicode/utf16.h>

namespace WebKit {

static bool useSubpixelPositioning = false;

void WebFontInfo::setSubpixelPositioning(bool subpixelPositioning)
{
    useSubpixelPositioning = subpixelPositioning;
}

class CachedFont {
public:
    // Note: We pass the charset explicitly as callers
    // should not create CachedFont entries without knowing
    // that the FcPattern contains a valid charset.
    CachedFont(FcPattern* pattern, FcCharSet* charSet)
        : m_supportedCharacters(charSet)
    {
        ASSERT(pattern);
        ASSERT(charSet);
        m_family.name = fontName(pattern);
        m_family.isBold = fontIsBold(pattern);
        m_family.isItalic = fontIsItalic(pattern);
    }
    const WebFontFamily& family() const { return m_family; }
    bool hasGlyphForCharacter(WebUChar32 c)
    {
        return m_supportedCharacters && FcCharSetHasChar(m_supportedCharacters, c);
    }

private:
    static WebCString fontName(FcPattern* pattern)
    {
        FcChar8* familyName;
        if (FcPatternGetString(pattern, FC_FAMILY, 0, &familyName) != FcResultMatch)
            return WebCString();

        // FCChar8 is unsigned char, so we cast to char for WebCString.
        const char* charFamily = reinterpret_cast<char*>(familyName);
        // FIXME: This should use WebString instead, and we should be
        // explicit about which encoding we're using. Right now callers
        // assume that this is utf8, but that may be wrong!
        return WebCString(charFamily, strlen(charFamily));
    }

    static bool fontIsBold(FcPattern* pattern)
    {
        int weight;
        if (FcPatternGetInteger(pattern, FC_WEIGHT, 0, &weight) != FcResultMatch)
            return false;
        return weight >= FC_WEIGHT_BOLD;
    }

    static bool fontIsItalic(FcPattern* pattern)
    {
        int slant;
        if (FcPatternGetInteger(pattern, FC_SLANT, 0, &slant) != FcResultMatch)
            return false;
        return slant != FC_SLANT_ROMAN;
    }

    WebFontFamily m_family;
    // m_supportedCharaters is owned by the parent
    // FcFontSet and should never be freed.
    FcCharSet* m_supportedCharacters;
};


class CachedFontSet {
    WTF_MAKE_NONCOPYABLE(CachedFontSet);
public:
    // CachedFontSet takes ownership of the passed FcFontSet.
    static PassOwnPtr<CachedFontSet> createForLocale(const char* locale)
    {
        FcFontSet* fontSet = createFcFontSetForLocale(locale);
        return adoptPtr(new CachedFontSet(fontSet));
    }

    ~CachedFontSet()
    {
        m_fallbackList.clear();
        FcFontSetDestroy(m_fontSet);
    }

    WebFontFamily familyForChar(WebUChar32 c)
    {
        Vector<CachedFont>::iterator itr = m_fallbackList.begin();
        for (; itr != m_fallbackList.end(); itr++) {
            if (itr->hasGlyphForCharacter(c))
                return itr->family();
        }
        // The previous code just returned garbage if the user didn't
        // have the necessary fonts, this seems better than garbage.
        // Current callers happen to ignore any values with an empty family string.
        return WebFontFamily();
    }

private:
    static FcFontSet* createFcFontSetForLocale(const char* locale)
    {
        ASSERT(locale);
        FcPattern* pattern = FcPatternCreate();
        // FcChar* is unsigned char* so we have to cast.
        FcPatternAddString(pattern, FC_LANG, reinterpret_cast<const FcChar8*>(locale));
        FcConfigSubstitute(0, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);

        // The result parameter returns if any fonts were found.
        // We already handle empty FcFontSets correctly, so we ignore the param.
        FcResult result;
        // Unfortunately we cannot use "trim" as FCFontSort ignores
        // FC_SCALABLE and we don't want bitmap fonts. If we used
        // trim it might trim scalable fonts which only added characters
        // covered by an earlier bitmap font. Instead we trim ourselves
        // in fillFallbackList until FcFontSort is fixed:
        // https://bugs.freedesktop.org/show_bug.cgi?id=67845
        FcFontSet* fontSet = FcFontSort(0, pattern, FcFalse, 0, &result);
        FcPatternDestroy(pattern);

        // The caller will take ownership of this FcFontSet.
        return fontSet;
    }

    CachedFontSet(FcFontSet* fontSet)
        : m_fontSet(fontSet)
    {
        fillFallbackList();
    }

    void fillFallbackList()
    {
        ASSERT(m_fallbackList.isEmpty());
        if (!m_fontSet)
            return;

        FcCharSet* allGlyphs = FcCharSetCreate();

        for (int i = 0; i < m_fontSet->nfont; ++i) {
            FcPattern* pattern = m_fontSet->fonts[i];

            // Ignore any bitmap fonts users may still have installed from last century.
            FcBool isScalable;
            if (FcPatternGetBool(pattern, FC_SCALABLE, 0, &isScalable) != FcResultMatch
                || !isScalable)
                continue;

            // Ignore any fonts FontConfig knows about, but that we don't have persmision to read.
            FcChar8* cFilename;
            if (FcPatternGetString(pattern, FC_FILE, 0, &cFilename) != FcResultMatch)
                continue;
            if (access(reinterpret_cast<char*>(cFilename), R_OK))
                continue;

            // Make sure this font can tell us what characters it has glyphs for.
            FcCharSet* charSet;
            if (FcPatternGetCharSet(pattern, FC_CHARSET, 0, &charSet) != FcResultMatch)
                continue;

            FcBool addsGlyphs = FcFalse;
            // FcCharSetMerge returns false on failure (out of memory, etc.)
            if (!FcCharSetMerge(allGlyphs, charSet, &addsGlyphs) || addsGlyphs)
                m_fallbackList.append(CachedFont(pattern, charSet));
        }
        // FIXME: We could cache allGlyphs, or even send it to the renderer
        // to avoid it needing to call us for glyphs we won't have.
        FcCharSetDestroy(allGlyphs);
    }

    FcFontSet* m_fontSet; // Owned by this object.
    // CachedFont has a FcCharset* which points into the FcFontSet.
    // If the FcFontSet is ever destoryed, the fallbackList
    // must be cleared first.
    Vector<CachedFont> m_fallbackList;
};

class FontSetCache {
public:
    static FontSetCache& shared()
    {
        DEFINE_STATIC_LOCAL(FontSetCache, cache, ());
        return cache;
    }

    WebFontFamily fontFamilyForCharInLocale(WebUChar32 c, const char* locale)
    {
        LocaleToCachedFont::iterator itr = m_setsByLocale.find(locale);
        if (itr == m_setsByLocale.end()) {
            OwnPtr<CachedFontSet> newEntry = CachedFontSet::createForLocale(locale);
            itr = m_setsByLocale.add(locale, newEntry.release()).iterator;
        }
        return itr.get()->value->familyForChar(c);
    }
    // FIXME: We may wish to add a way to prune the cache at a later time.

private:
    // FIXME: This shouldn't need to be AtomicString, but
    // currently HashTraits<const char*> isn't smart enough
    // to hash the string (only does pointer compares).
    typedef HashMap<AtomicString, OwnPtr<CachedFontSet> > LocaleToCachedFont;
    LocaleToCachedFont m_setsByLocale;
};

void WebFontInfo::familyForChar(WebUChar32 c, const char* locale, WebFontFamily* family)
{
    *family = FontSetCache::shared().fontFamilyForCharInLocale(c, locale);
}

void WebFontInfo::renderStyleForStrike(const char* family, int sizeAndStyle, WebFontRenderStyle* out)
{
    bool isBold = sizeAndStyle & 1;
    bool isItalic = sizeAndStyle & 2;
    int pixelSize = sizeAndStyle >> 2;

    FcPattern* pattern = FcPatternCreate();
    FcValue fcvalue;

    fcvalue.type = FcTypeString;
    fcvalue.u.s = reinterpret_cast<const FcChar8 *>(family);
    FcPatternAdd(pattern, FC_FAMILY, fcvalue, FcFalse);

    fcvalue.type = FcTypeInteger;
    fcvalue.u.i = isBold ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL;
    FcPatternAdd(pattern, FC_WEIGHT, fcvalue, FcFalse);

    fcvalue.type = FcTypeInteger;
    fcvalue.u.i = isItalic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
    FcPatternAdd(pattern, FC_SLANT, fcvalue, FcFalse);

    fcvalue.type = FcTypeBool;
    fcvalue.u.b = FcTrue;
    FcPatternAdd(pattern, FC_SCALABLE, fcvalue, FcFalse);

    fcvalue.type = FcTypeDouble;
    fcvalue.u.d = pixelSize;
    FcPatternAdd(pattern, FC_SIZE, fcvalue, FcFalse);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    // Some versions of fontconfig don't actually write a value into result.
    // However, it's not clear from the documentation if result should be a
    // non-0 pointer: future versions might expect to be able to write to
    // it. So we pass in a valid pointer and ignore it.
    FcPattern* match = FcFontMatch(0, pattern, &result);
    FcPatternDestroy(pattern);

    out->setDefaults();

    if (!match)
        return;

    FcBool b;
    int i;

    if (FcPatternGetBool(match, FC_ANTIALIAS, 0, &b) == FcResultMatch)
        out->useAntiAlias = b;
    if (FcPatternGetBool(match, FC_EMBEDDED_BITMAP, 0, &b) == FcResultMatch)
        out->useBitmaps = b;
    if (FcPatternGetBool(match, FC_AUTOHINT, 0, &b) == FcResultMatch)
        out->useAutoHint = b;
    if (FcPatternGetBool(match, FC_HINTING, 0, &b) == FcResultMatch)
        out->useHinting = b;
    if (FcPatternGetInteger(match, FC_HINT_STYLE, 0, &i) == FcResultMatch)
        out->hintStyle = i;
    if (FcPatternGetInteger(match, FC_RGBA, 0, &i) == FcResultMatch) {
        switch (i) {
        case FC_RGBA_NONE:
            out->useSubpixelRendering = 0;
            break;
        case FC_RGBA_RGB:
        case FC_RGBA_BGR:
        case FC_RGBA_VRGB:
        case FC_RGBA_VBGR:
            out->useSubpixelRendering = 1;
            break;
        default:
            // This includes FC_RGBA_UNKNOWN.
            out->useSubpixelRendering = 2;
            break;
        }
    }

    // FontConfig doesn't provide parameters to configure whether subpixel
    // positioning should be used or not, so we just use a global setting.
    out->useSubpixelPositioning = useSubpixelPositioning;

    FcPatternDestroy(match);
}

} // namespace WebKit
