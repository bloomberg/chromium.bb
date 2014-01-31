/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "platform/fonts/Font.h"

#include "platform/fonts/Character.h"
#include "platform/fonts/FontPlatformFeatures.h"
#include "platform/fonts/WidthIterator.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "wtf/MainThread.h"
#include "wtf/StdLibExtras.h"

using namespace WTF;
using namespace Unicode;

namespace WebCore {

CodePath Font::s_codePath = AutoPath;

TypesettingFeatures Font::s_defaultTypesettingFeatures = 0;

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font()
    : m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_typesettingFeatures(0)
{
}

Font::Font(const FontDescription& fd, float letterSpacing, float wordSpacing)
    : m_fontDescription(fd)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_typesettingFeatures(computeTypesettingFeatures())
{
}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_fontFallbackList(other.m_fontFallbackList)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_typesettingFeatures(computeTypesettingFeatures())
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontFallbackList = other.m_fontFallbackList;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_typesettingFeatures = other.m_typesettingFeatures;
    return *this;
}

bool Font::operator==(const Font& other) const
{
    // Our FontData don't have to be checked, since checking the font description will be fine.
    // FIXME: This does not work if the font was made with the FontPlatformData constructor.
    if (loadingCustomFonts() || other.loadingCustomFonts())
        return false;

    FontSelector* first = m_fontFallbackList ? m_fontFallbackList->fontSelector() : 0;
    FontSelector* second = other.m_fontFallbackList ? other.m_fontFallbackList->fontSelector() : 0;

    return first == second
        && m_fontDescription == other.m_fontDescription
        && m_letterSpacing == other.m_letterSpacing
        && m_wordSpacing == other.m_wordSpacing
        && (m_fontFallbackList ? m_fontFallbackList->fontSelectorVersion() : 0) == (other.m_fontFallbackList ? other.m_fontFallbackList->fontSelectorVersion() : 0)
        && (m_fontFallbackList ? m_fontFallbackList->generation() : 0) == (other.m_fontFallbackList ? other.m_fontFallbackList->generation() : 0);
}

void Font::update(PassRefPtr<FontSelector> fontSelector) const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway. Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble). Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontFallbackList)
        m_fontFallbackList = FontFallbackList::create();
    m_fontFallbackList->invalidate(fontSelector);
    m_typesettingFeatures = computeTypesettingFeatures();
}

void Font::drawText(GraphicsContext* context, const TextRunPaintInfo& runInfo, const FloatPoint& point, CustomFontNotReadyAction customFontNotReadyAction) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading,
    // except if the 'force' argument is set to true (in which case it will use a fallback
    // font).
    if (loadingCustomFonts() && customFontNotReadyAction == DoNotPaintIfFontNotReady)
        return;

    CodePath codePathToUse = codePath(runInfo.run);
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePathToUse != ComplexPath && typesettingFeatures() && (runInfo.from || runInfo.to != runInfo.run.length()))
        codePathToUse = ComplexPath;

    if (codePathToUse != ComplexPath)
        return drawSimpleText(context, runInfo, point);

    return drawComplexText(context, runInfo, point);
}

void Font::drawEmphasisMarks(GraphicsContext* context, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point) const
{
    if (loadingCustomFonts())
        return;

    CodePath codePathToUse = codePath(runInfo.run);
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePathToUse != ComplexPath && typesettingFeatures() && (runInfo.from || runInfo.to != runInfo.run.length()))
        codePathToUse = ComplexPath;

    if (codePathToUse != ComplexPath)
        drawEmphasisMarksForSimpleText(context, runInfo, mark, point);
    else
        drawEmphasisMarksForComplexText(context, runInfo, mark, point);
}

float Font::width(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    CodePath codePathToUse = codePath(run);
    if (codePathToUse != ComplexPath) {
        // The complex path is more restrictive about returning fallback fonts than the simple path, so we need an explicit test to make their behaviors match.
        if (!FontPlatformFeatures::canReturnFallbackFontsForComplexText())
            fallbackFonts = 0;
        // The simple path can optimize the case where glyph overflow is not observable.
        if (codePathToUse != SimpleWithGlyphOverflowPath && (glyphOverflow && !glyphOverflow->computeBounds))
            glyphOverflow = 0;
    }

    bool hasKerningOrLigatures = typesettingFeatures() & (Kerning | Ligatures);
    bool hasWordSpacingOrLetterSpacing = wordSpacing() || letterSpacing();
    float* cacheEntry = m_fontFallbackList->widthCache().add(run, std::numeric_limits<float>::quiet_NaN(), hasKerningOrLigatures, hasWordSpacingOrLetterSpacing, glyphOverflow);
    if (cacheEntry && !std::isnan(*cacheEntry))
        return *cacheEntry;

    float result;
    if (codePathToUse == ComplexPath)
        result = floatWidthForComplexText(run, fallbackFonts, glyphOverflow);
    else
        result = floatWidthForSimpleText(run, fallbackFonts, glyphOverflow);

    if (cacheEntry && (!fallbackFonts || fallbackFonts->isEmpty()))
        *cacheEntry = result;
    return result;
}

float Font::width(const TextRun& run, int& charsConsumed, String& glyphName) const
{
#if ENABLE(SVG_FONTS)
    if (TextRun::RenderingContext* renderingContext = run.renderingContext())
        return renderingContext->floatWidthUsingSVGFont(*this, run, charsConsumed, glyphName);
#endif

    charsConsumed = run.length();
    glyphName = "";
    return width(run);
}

FloatRect Font::selectionRectForText(const TextRun& run, const FloatPoint& point, int h, int from, int to) const
{
    to = (to == -1 ? run.length() : to);

    CodePath codePathToUse = codePath(run);
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePathToUse != ComplexPath && typesettingFeatures() && (from || to != run.length()))
        codePathToUse = ComplexPath;

    if (codePathToUse != ComplexPath)
        return selectionRectForSimpleText(run, point, h, from, to);

    return selectionRectForComplexText(run, point, h, from, to);
}

int Font::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePath(run) != ComplexPath && !typesettingFeatures())
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

static bool shouldUseFontSmoothing = true;

void Font::setShouldUseSmoothing(bool shouldUseSmoothing)
{
    ASSERT(isMainThread());
    shouldUseFontSmoothing = shouldUseSmoothing;
}

bool Font::shouldUseSmoothing()
{
    return shouldUseFontSmoothing;
}

void Font::setCodePath(CodePath p)
{
    s_codePath = p;
}

CodePath Font::codePath()
{
    return s_codePath;
}

void Font::setDefaultTypesettingFeatures(TypesettingFeatures typesettingFeatures)
{
    s_defaultTypesettingFeatures = typesettingFeatures;
}

TypesettingFeatures Font::defaultTypesettingFeatures()
{
    return s_defaultTypesettingFeatures;
}

CodePath Font::codePath(const TextRun& run) const
{
    if (s_codePath != AutoPath)
        return s_codePath;

#if ENABLE(SVG_FONTS)
    if (run.renderingContext())
        return SimplePath;
#endif

    if (m_fontDescription.featureSettings() && m_fontDescription.featureSettings()->size() > 0)
        return ComplexPath;

    if (run.length() > 1 && !WidthIterator::supportsTypesettingFeatures(*this))
        return ComplexPath;

    if (!run.characterScanForCodePath())
        return SimplePath;

    if (run.is8Bit())
        return SimplePath;

    // Start from 0 since drawing and highlighting also measure the characters before run->from.
    return Character::characterRangeCodePath(run.characters16(), run.length());
}

void Font::willUseFontData() const
{
    const FontFamily& family = fontDescription().family();
    if (m_fontFallbackList && m_fontFallbackList->fontSelector() && !family.familyIsEmpty())
        m_fontFallbackList->fontSelector()->willUseFontData(fontDescription(), family.family());
}

}
