/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (c) 2007, 2008, 2010 Google Inc. All rights reserved.
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

#include "SkPaint.h"
#include "SkTemplates.h"
#include "platform/LayoutUnit.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Character.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/GlyphPageTreeNode.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/SimpleShaper.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"
#include "wtf/MainThread.h"
#include "wtf/StdLibExtras.h"
#include "wtf/unicode/CharacterNames.h"
#include "wtf/unicode/Unicode.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

Font::Font()
{
}

Font::Font(const FontDescription& fd)
    : m_fontDescription(fd)
{
}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_fontFallbackList(other.m_fontFallbackList)
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fontFallbackList = other.m_fontFallbackList;
    return *this;
}

bool Font::operator==(const Font& other) const
{
    FontSelector* first = m_fontFallbackList ? m_fontFallbackList->fontSelector() : 0;
    FontSelector* second = other.m_fontFallbackList ? other.m_fontFallbackList->fontSelector() : 0;

    return first == second
        && m_fontDescription == other.m_fontDescription
        && (m_fontFallbackList ? m_fontFallbackList->fontSelectorVersion() : 0) == (other.m_fontFallbackList ? other.m_fontFallbackList->fontSelectorVersion() : 0)
        && (m_fontFallbackList ? m_fontFallbackList->generation() : 0) == (other.m_fontFallbackList ? other.m_fontFallbackList->generation() : 0);
}

void Font::update(PassRefPtrWillBeRawPtr<FontSelector> fontSelector) const
{
    // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr, but it ends up
    // being reasonably safe (because inherited fonts in the render tree pick up the new
    // style anyway. Other copies are transient, e.g., the state in the GraphicsContext, and
    // won't stick around long enough to get you in trouble). Still, this is pretty disgusting,
    // and could eventually be rectified by using RefPtrs for Fonts themselves.
    if (!m_fontFallbackList)
        m_fontFallbackList = FontFallbackList::create();
    m_fontFallbackList->invalidate(fontSelector);
}

float Font::buildGlyphBuffer(const TextRunPaintInfo& runInfo, GlyphBuffer& glyphBuffer,
    const GlyphData* emphasisData) const
{
    if (codePath(runInfo) == ComplexPath) {
        HarfBuzzShaper shaper(this, runInfo.run, emphasisData);
        shaper.setDrawRange(runInfo.from, runInfo.to);
        shaper.shape(&glyphBuffer);
        return shaper.totalWidth();
    }

    SimpleShaper shaper(this, runInfo.run, emphasisData, nullptr /* fallbackFonts */, nullptr);
    shaper.advance(runInfo.from);
    shaper.advance(runInfo.to, &glyphBuffer);
    float width = shaper.runWidthSoFar();

    if (runInfo.run.rtl()) {
        // Glyphs are shaped & stored in RTL advance order - reverse them for LTR drawing.
        shaper.advance(runInfo.run.length());
        glyphBuffer.reverseForSimpleRTL(width, shaper.runWidthSoFar());
    }

    return width;
}

void Font::drawText(GraphicsContext* context, const TextRunPaintInfo& runInfo,
    const FloatPoint& point) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading.
    if (shouldSkipDrawing())
        return;

    TextDrawingModeFlags textMode = context->textDrawingMode();
    if (!(textMode & TextModeFill) && !((textMode & TextModeStroke) && context->hasStroke()))
        return;

    if (runInfo.cachedTextBlob && runInfo.cachedTextBlob->get()) {
        ASSERT(RuntimeEnabledFeatures::textBlobEnabled());
        // we have a pre-cached blob -- happy joy!
        drawTextBlob(context, runInfo.cachedTextBlob->get(), point.data());
        return;
    }

    GlyphBuffer glyphBuffer;
    buildGlyphBuffer(runInfo, glyphBuffer);

    drawGlyphBuffer(context, runInfo, glyphBuffer, point);
}

void Font::drawBidiText(GraphicsContext* context, const TextRunPaintInfo& runInfo,
    const FloatPoint& point, CustomFontNotReadyAction customFontNotReadyAction) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading,
    // except if the 'force' argument is set to true (in which case it will use a fallback
    // font).
    if (shouldSkipDrawing() && customFontNotReadyAction == DoNotPaintIfFontNotReady)
        return;

    TextDrawingModeFlags textMode = context->textDrawingMode();
    if (!(textMode & TextModeFill) && !((textMode & TextModeStroke) && context->hasStroke()))
        return;

    // sub-run painting is not supported for Bidi text.
    const TextRun& run = runInfo.run;
    ASSERT((runInfo.from == 0) && (runInfo.to == run.length()));
    BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
    bidiResolver.setStatus(BidiStatus(run.direction(), run.directionalOverride()));
    bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

    // FIXME: This ownership should be reversed. We should pass BidiRunList
    // to BidiResolver in createBidiRunsForLine.
    BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
    bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));
    if (!bidiRuns.runCount())
        return;

    FloatPoint currPoint = point;
    BidiCharacterRun* bidiRun = bidiRuns.firstRun();
    while (bidiRun) {
        TextRun subrun = run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
        bool isRTL = bidiRun->level() % 2;
        subrun.setDirection(isRTL ? RTL : LTR);
        subrun.setDirectionalOverride(bidiRun->dirOverride(false));

        TextRunPaintInfo subrunInfo(subrun);
        subrunInfo.bounds = runInfo.bounds;

        // TODO: investigate blob consolidation/caching (technically,
        //       all subruns could be part of the same blob).
        GlyphBuffer glyphBuffer;
        float runWidth = buildGlyphBuffer(subrunInfo, glyphBuffer);
        drawGlyphBuffer(context, subrunInfo, glyphBuffer, point);

        bidiRun = bidiRun->next();
        currPoint.move(runWidth, 0);
    }

    bidiRuns.deleteRuns();
}

void Font::drawEmphasisMarks(GraphicsContext* context, const TextRunPaintInfo& runInfo, const AtomicString& mark, const FloatPoint& point) const
{
    if (shouldSkipDrawing())
        return;

    FontCachePurgePreventer purgePreventer;

    GlyphData emphasisGlyphData;
    if (!getEmphasisMarkGlyphData(mark, emphasisGlyphData))
        return;

    ASSERT(emphasisGlyphData.fontData);
    if (!emphasisGlyphData.fontData)
        return;

    GlyphBuffer glyphBuffer;
    buildGlyphBuffer(runInfo, glyphBuffer, &emphasisGlyphData);

    if (glyphBuffer.isEmpty())
        return;

    drawGlyphBuffer(context, runInfo, glyphBuffer, point);
}

static inline void updateGlyphOverflowFromBounds(const IntRectOutsets& glyphBounds,
    const FontMetrics& fontMetrics, GlyphOverflow* glyphOverflow)
{
    glyphOverflow->top = std::max<int>(glyphOverflow->top,
        glyphBounds.top() - (glyphOverflow->computeBounds ? 0 : fontMetrics.ascent()));
    glyphOverflow->bottom = std::max<int>(glyphOverflow->bottom,
        glyphBounds.bottom() - (glyphOverflow->computeBounds ? 0 : fontMetrics.descent()));
    glyphOverflow->left = glyphBounds.left();
    glyphOverflow->right = glyphBounds.right();
}

float Font::width(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    FontCachePurgePreventer purgePreventer;

    CodePath codePathToUse = codePath(TextRunPaintInfo(run));
    if (codePathToUse != ComplexPath) {
        // The simple path can optimize the case where glyph overflow is not observable.
        if (codePathToUse != SimpleWithGlyphOverflowPath && (glyphOverflow && !glyphOverflow->computeBounds))
            glyphOverflow = 0;
    }

    bool hasWordSpacingOrLetterSpacing = fontDescription().wordSpacing() || fontDescription().letterSpacing();
    bool isCacheable = codePathToUse == ComplexPath
        && !hasWordSpacingOrLetterSpacing // Word spacing and letter spacing can change the width of a word.
        && !run.allowTabs(); // If we allow tabs and a tab occurs inside a word, the width of the word varies based on its position on the line.

    WidthCacheEntry* cacheEntry = isCacheable
        ? m_fontFallbackList->widthCache().add(run, WidthCacheEntry())
        : 0;
    if (cacheEntry && cacheEntry->isValid()) {
        if (glyphOverflow)
            updateGlyphOverflowFromBounds(cacheEntry->glyphBounds, fontMetrics(), glyphOverflow);
        return cacheEntry->width;
    }

    float result;
    IntRectOutsets glyphBounds;
    if (codePathToUse == ComplexPath) {
        result = floatWidthForComplexText(run, fallbackFonts, &glyphBounds);
    } else {
        ASSERT(!isCacheable);
        result = floatWidthForSimpleText(run, fallbackFonts, glyphOverflow ? &glyphBounds : 0);
    }

    if (cacheEntry && (!fallbackFonts || fallbackFonts->isEmpty())) {
        cacheEntry->glyphBounds = glyphBounds;
        cacheEntry->width = result;
    }

    if (glyphOverflow)
        updateGlyphOverflowFromBounds(glyphBounds, fontMetrics(), glyphOverflow);
    return result;
}

PassTextBlobPtr Font::buildTextBlob(const GlyphBuffer& glyphBuffer) const
{
    ASSERT(RuntimeEnabledFeatures::textBlobEnabled());

    SkTextBlobBuilder builder;
    bool hasVerticalOffsets = glyphBuffer.hasVerticalOffsets();

    unsigned i = 0;
    while (i < glyphBuffer.size()) {
        const SimpleFontData* fontData = glyphBuffer.fontDataAt(i);

        // FIXME: Handle vertical text.
        if (fontData->platformData().orientation() == Vertical)
            return nullptr;

        SkPaint paint;
        // FIXME: FontPlatformData makes some decisions on the device scale
        // factor, which is found via the GraphicsContext. This should be fixed
        // to avoid correctness problems here.
        float deviceScaleFactor = 1.0f;
        fontData->platformData().setupPaint(&paint, deviceScaleFactor, this);
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

        unsigned start = i++;
        while (i < glyphBuffer.size() && glyphBuffer.fontDataAt(i) == fontData)
            i++;
        unsigned count = i - start;

        const SkTextBlobBuilder::RunBuffer& buffer = hasVerticalOffsets
            ? builder.allocRunPos(paint, count)
            : builder.allocRunPosH(paint, count, 0);

        const uint16_t* glyphs = glyphBuffer.glyphs(start);
        const float* offsets = glyphBuffer.offsets(start);
        std::copy(glyphs, glyphs + count, buffer.glyphs);
        std::copy(offsets, offsets + (hasVerticalOffsets ? 2 * count : count), buffer.pos);
    }

    return adoptRef(builder.build());
}

static inline FloatRect pixelSnappedSelectionRect(FloatRect rect)
{
    // Using roundf() rather than ceilf() for the right edge as a compromise to
    // ensure correct caret positioning.
    float roundedX = roundf(rect.x());
    return FloatRect(roundedX, rect.y(), roundf(rect.maxX() - roundedX), rect.height());
}

FloatRect Font::selectionRectForText(const TextRun& run, const FloatPoint& point, int h, int from, int to, bool accountForGlyphBounds) const
{
    to = (to == -1 ? run.length() : to);

    TextRunPaintInfo runInfo(run);
    runInfo.from = from;
    runInfo.to = to;

    FontCachePurgePreventer purgePreventer;

    if (codePath(runInfo) != ComplexPath)
        return pixelSnappedSelectionRect(selectionRectForSimpleText(run, point, h, from, to, accountForGlyphBounds));
    return pixelSnappedSelectionRect(selectionRectForComplexText(run, point, h, from, to));
}

int Font::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
    FontCachePurgePreventer purgePreventer;

    if (codePath(TextRunPaintInfo(run)) != ComplexPath && !fontDescription().typesettingFeatures())
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

CodePath Font::codePath(const TextRunPaintInfo& runInfo) const
{
    const TextRun& run = runInfo.run;

    if (fontDescription().typesettingFeatures() && (runInfo.from || runInfo.to != run.length()))
        return ComplexPath;

    if (m_fontDescription.featureSettings() && m_fontDescription.featureSettings()->size() > 0 && m_fontDescription.letterSpacing() == 0)
        return ComplexPath;

    if (m_fontDescription.orientation() == Vertical)
        return ComplexPath;

    if (m_fontDescription.widthVariant() != RegularWidth)
        return ComplexPath;

    if (run.length() > 1 && fontDescription().typesettingFeatures())
        return ComplexPath;

    // FIXME: This really shouldn't be needed but for some reason the
    // TextRendering setting doesn't propagate to typesettingFeatures in time
    // for the prefs width calculation.
    if (fontDescription().textRendering() == OptimizeLegibility || fontDescription().textRendering() == GeometricPrecision)
        return ComplexPath;

    if (run.codePath() == TextRun::ForceComplex)
        return ComplexPath;

    if (run.codePath() == TextRun::ForceSimple)
        return SimplePath;

    if (run.is8Bit())
        return SimplePath;

    // Start from 0 since drawing and highlighting also measure the characters before run->from.
    return Character::characterRangeCodePath(run.characters16(), run.length());
}

void Font::willUseFontData(UChar32 character) const
{
    const FontFamily& family = fontDescription().family();
    if (m_fontFallbackList && m_fontFallbackList->fontSelector() && !family.familyIsEmpty())
        m_fontFallbackList->fontSelector()->willUseFontData(fontDescription(), family.family(), character);
}

static inline GlyphData glyphDataForNonCJKCharacterWithGlyphOrientation(UChar32 character, NonCJKGlyphOrientation orientation, GlyphData& data, unsigned pageNumber)
{
    if (orientation == NonCJKGlyphOrientationUpright || Character::shouldIgnoreRotation(character)) {
        RefPtr<SimpleFontData> uprightFontData = data.fontData->uprightOrientationFontData();
        GlyphPageTreeNode* uprightNode = GlyphPageTreeNode::getNormalRootChild(uprightFontData.get(), pageNumber);
        GlyphPage* uprightPage = uprightNode->page();
        if (uprightPage) {
            GlyphData uprightData = uprightPage->glyphDataForCharacter(character);
            // If the glyphs are the same, then we know we can just use the horizontal glyph rotated vertically to be upright.
            if (data.glyph == uprightData.glyph)
                return data;
            // The glyphs are distinct, meaning that the font has a vertical-right glyph baked into it. We can't use that
            // glyph, so we fall back to the upright data and use the horizontal glyph.
            if (uprightData.fontData)
                return uprightData;
        }
    } else if (orientation == NonCJKGlyphOrientationVerticalRight) {
        RefPtr<SimpleFontData> verticalRightFontData = data.fontData->verticalRightOrientationFontData();
        GlyphPageTreeNode* verticalRightNode = GlyphPageTreeNode::getNormalRootChild(verticalRightFontData.get(), pageNumber);
        GlyphPage* verticalRightPage = verticalRightNode->page();
        if (verticalRightPage) {
            GlyphData verticalRightData = verticalRightPage->glyphDataForCharacter(character);
            // If the glyphs are distinct, we will make the assumption that the font has a vertical-right glyph baked
            // into it.
            if (data.glyph != verticalRightData.glyph)
                return data;
            // The glyphs are identical, meaning that we should just use the horizontal glyph.
            if (verticalRightData.fontData)
                return verticalRightData;
        }
    }
    return data;
}

GlyphData Font::glyphDataForCharacter(UChar32& c, bool mirror, bool normalizeSpace, FontDataVariant variant) const
{
    ASSERT(isMainThread());

    if (variant == AutoVariant) {
        if (m_fontDescription.variant() == FontVariantSmallCaps) {
            UChar32 upperC = toUpper(c);
            if (upperC != c) {
                c = upperC;
                variant = SmallCapsVariant;
            } else {
                variant = NormalVariant;
            }
        } else {
            variant = NormalVariant;
        }
    }

    if (normalizeSpace && Character::isNormalizedCanvasSpaceCharacter(c))
        c = space;

    if (mirror)
        c = mirroredChar(c);

    unsigned pageNumber = (c / GlyphPage::size);

    GlyphPageTreeNodeBase* node = m_fontFallbackList->getPageNode(pageNumber);
    if (!node) {
        node = GlyphPageTreeNode::getRootChild(fontDataAt(0), pageNumber);
        m_fontFallbackList->setPageNode(pageNumber, node);
    }

    GlyphPage* page = 0;
    if (variant == NormalVariant) {
        // Fastest loop, for the common case (normal variant).
        while (true) {
            page = node->page(m_fontDescription.script());
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData && (data.fontData->platformData().orientation() == Horizontal || data.fontData->isTextOrientationFallback()))
                    return data;

                if (data.fontData) {
                    if (Character::isCJKIdeographOrSymbol(c)) {
                        if (!data.fontData->hasVerticalGlyphs()) {
                            // Use the broken ideograph font data. The broken ideograph font will use the horizontal width of glyphs
                            // to make sure you get a square (even for broken glyphs like symbols used for punctuation).
                            variant = BrokenIdeographVariant;
                            break;
                        }
                    } else {
                        return glyphDataForNonCJKCharacterWithGlyphOrientation(c, m_fontDescription.nonCJKGlyphOrientation(), data, pageNumber);
                    }

                    return data;
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = toGlyphPageTreeNode(node)->getChild(fontDataAt(node->level()), pageNumber);
            m_fontFallbackList->setPageNode(pageNumber, node);
        }
    }
    if (variant != NormalVariant) {
        while (true) {
            page = node->page(m_fontDescription.script());
            if (page) {
                GlyphData data = page->glyphDataForCharacter(c);
                if (data.fontData) {
                    // The variantFontData function should not normally return 0.
                    // But if it does, we will just render the capital letter big.
                    RefPtr<SimpleFontData> variantFontData = data.fontData->variantFontData(m_fontDescription, variant);
                    if (!variantFontData)
                        return data;

                    GlyphPageTreeNode* variantNode = GlyphPageTreeNode::getNormalRootChild(variantFontData.get(), pageNumber);
                    GlyphPage* variantPage = variantNode->page();
                    if (variantPage) {
                        GlyphData data = variantPage->glyphDataForCharacter(c);
                        if (data.fontData)
                            return data;
                    }

                    // Do not attempt system fallback off the variantFontData. This is the very unlikely case that
                    // a font has the lowercase character but the small caps font does not have its uppercase version.
                    return variantFontData->missingGlyphData();
                }

                if (node->isSystemFallback())
                    break;
            }

            // Proceed with the fallback list.
            node = toGlyphPageTreeNode(node)->getChild(fontDataAt(node->level()), pageNumber);
            m_fontFallbackList->setPageNode(pageNumber, node);
        }
    }

    ASSERT(page);
    ASSERT(node->isSystemFallback());

    // System fallback is character-dependent. When we get here, we
    // know that the character in question isn't in the system fallback
    // font's glyph page. Try to lazily create it here.

    // FIXME: Unclear if this should normalizeSpaces above 0xFFFF.
    // Doing so changes fast/text/international/plane2-diffs.html
    UChar32 characterToRender = c;
    if (characterToRender <=  0xFFFF)
        characterToRender = Character::normalizeSpaces(characterToRender);

    const FontData* fontData = fontDataAt(0);
    if (fontData) {
        const SimpleFontData* fontDataToSubstitute = fontData->fontDataForCharacter(characterToRender);
        RefPtr<SimpleFontData> characterFontData = FontCache::fontCache()->fallbackFontForCharacter(m_fontDescription, characterToRender, fontDataToSubstitute);
        if (characterFontData) {
            if (characterFontData->platformData().orientation() == Vertical && !characterFontData->hasVerticalGlyphs() && Character::isCJKIdeographOrSymbol(c))
                variant = BrokenIdeographVariant;
            if (variant != NormalVariant)
                characterFontData = characterFontData->variantFontData(m_fontDescription, variant);
        }
        if (characterFontData) {
            // Got the fallback glyph and font.
            GlyphPage* fallbackPage = GlyphPageTreeNode::getRootChild(characterFontData.get(), pageNumber)->page();
            GlyphData data = fallbackPage && fallbackPage->glyphForCharacter(c) ? fallbackPage->glyphDataForCharacter(c) : characterFontData->missingGlyphData();
            // Cache it so we don't have to do system fallback again next time.
            if (variant == NormalVariant) {
                page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
                data.fontData->setMaxGlyphPageTreeLevel(std::max(data.fontData->maxGlyphPageTreeLevel(), node->level()));
                if (!Character::isCJKIdeographOrSymbol(c) && data.fontData->platformData().orientation() != Horizontal && !data.fontData->isTextOrientationFallback())
                    return glyphDataForNonCJKCharacterWithGlyphOrientation(c, m_fontDescription.nonCJKGlyphOrientation(), data, pageNumber);
            }
            return data;
        }
    }

    // Even system fallback can fail; use the missing glyph in that case.
    // FIXME: It would be nicer to use the missing glyph from the last resort font instead.
    ASSERT(primaryFont());
    GlyphData data = primaryFont()->missingGlyphData();
    if (variant == NormalVariant) {
        page->setGlyphDataForCharacter(c, data.glyph, data.fontData);
        data.fontData->setMaxGlyphPageTreeLevel(std::max(data.fontData->maxGlyphPageTreeLevel(), node->level()));
    }
    return data;
}

bool Font::primaryFontHasGlyphForCharacter(UChar32 character) const
{
    ASSERT(primaryFont());
    unsigned pageNumber = (character / GlyphPage::size);

    GlyphPageTreeNode* node = GlyphPageTreeNode::getNormalRootChild(primaryFont(), pageNumber);
    GlyphPage* page = node->page();

    return page && page->glyphForCharacter(character);
}

// FIXME: This function may not work if the emphasis mark uses a complex script, but none of the
// standard emphasis marks do so.
bool Font::getEmphasisMarkGlyphData(const AtomicString& mark, GlyphData& glyphData) const
{
    if (mark.isEmpty())
        return false;

    UChar32 character = mark[0];

    if (U16_IS_SURROGATE(character)) {
        if (!U16_IS_SURROGATE_LEAD(character))
            return false;

        if (mark.length() < 2)
            return false;

        UChar low = mark[1];
        if (!U16_IS_TRAIL(low))
            return false;

        character = U16_GET_SUPPLEMENTARY(character, low);
    }

    bool normalizeSpace = false;
    glyphData = glyphDataForCharacter(character, false, normalizeSpace, EmphasisMarkVariant);
    return true;
}

int Font::emphasisMarkAscent(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().ascent();
}

int Font::emphasisMarkDescent(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().descent();
}

int Font::emphasisMarkHeight(const AtomicString& mark) const
{
    FontCachePurgePreventer purgePreventer;

    GlyphData markGlyphData;
    if (!getEmphasisMarkGlyphData(mark, markGlyphData))
        return 0;

    const SimpleFontData* markFontData = markGlyphData.fontData;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().height();
}

SkPaint Font::textFillPaint(GraphicsContext* gc, const SimpleFontData* font) const
{
    SkPaint paint = gc->fillPaint();
    font->platformData().setupPaint(&paint, gc->deviceScaleFactor(), this);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    return paint;
}

SkPaint Font::textStrokePaint(GraphicsContext* gc, const SimpleFontData* font, bool isFilling) const
{
    SkPaint paint = gc->strokePaint();
    font->platformData().setupPaint(&paint, gc->deviceScaleFactor(), this);
    paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
    if (isFilling) {
        // If there is a shadow and we filled above, there will already be
        // a shadow. We don't want to draw it again or it will be too dark
        // and it will go on top of the fill.
        //
        // Note that this isn't strictly correct, since the stroke could be
        // very thick and the shadow wouldn't account for this. The "right"
        // thing would be to draw to a new layer and then draw that layer
        // with a shadow. But this is a lot of extra work for something
        // that isn't normally an issue.
        paint.setLooper(0);
    }
    return paint;
}

void Font::paintGlyphs(GraphicsContext* gc, const SimpleFontData* font,
    const Glyph glyphs[], unsigned numGlyphs,
    const SkPoint pos[], const FloatRect& textRect) const
{
    TextDrawingModeFlags textMode = gc->textDrawingMode();

    // We draw text up to two times (once for fill, once for stroke).
    if (textMode & TextModeFill) {
        SkPaint paint = textFillPaint(gc, font);
        gc->drawPosText(glyphs, numGlyphs * sizeof(Glyph), pos, textRect, paint);
    }

    if ((textMode & TextModeStroke) && gc->hasStroke()) {
        SkPaint paint = textStrokePaint(gc, font, textMode & TextModeFill);
        gc->drawPosText(glyphs, numGlyphs * sizeof(Glyph), pos, textRect, paint);
    }
}

void Font::paintGlyphsHorizontal(GraphicsContext* gc, const SimpleFontData* font,
    const Glyph glyphs[], unsigned numGlyphs,
    const SkScalar xpos[], SkScalar constY, const FloatRect& textRect) const
{
    TextDrawingModeFlags textMode = gc->textDrawingMode();

    if (textMode & TextModeFill) {
        SkPaint paint = textFillPaint(gc, font);
        gc->drawPosTextH(glyphs, numGlyphs * sizeof(Glyph), xpos, constY, textRect, paint);
    }

    if ((textMode & TextModeStroke) && gc->hasStroke()) {
        SkPaint paint = textStrokePaint(gc, font, textMode & TextModeFill);
        gc->drawPosTextH(glyphs, numGlyphs * sizeof(Glyph), xpos, constY, textRect, paint);
    }
}

void Font::drawGlyphs(GraphicsContext* gc, const SimpleFontData* font,
    const GlyphBuffer& glyphBuffer, unsigned from, unsigned numGlyphs,
    const FloatPoint& point, const FloatRect& textRect) const
{
    ASSERT(glyphBuffer.size() >= from + numGlyphs);

    if (!glyphBuffer.hasVerticalOffsets()) {
        SkAutoSTMalloc<64, SkScalar> storage(numGlyphs);
        SkScalar* xpos = storage.get();
        for (unsigned i = 0; i < numGlyphs; i++)
            xpos[i] = SkFloatToScalar(point.x() + glyphBuffer.xOffsetAt(from + i));

        paintGlyphsHorizontal(gc, font, glyphBuffer.glyphs(from), numGlyphs, xpos,
            SkFloatToScalar(point.y()), textRect);
        return;
    }

    bool drawVertically = font->platformData().orientation() == Vertical && font->verticalData();

    GraphicsContextStateSaver stateSaver(*gc, false);
    if (drawVertically) {
        stateSaver.save();
        gc->concatCTM(AffineTransform(0, -1, 1, 0, point.x(), point.y()));
        gc->concatCTM(AffineTransform(1, 0, 0, 1, -point.x(), -point.y()));
    }

    const float verticalBaselineXOffset = drawVertically ? SkFloatToScalar(font->fontMetrics().floatAscent() - font->fontMetrics().floatAscent(IdeographicBaseline)) : 0;

    ASSERT(glyphBuffer.hasVerticalOffsets());
    SkAutoSTMalloc<32, SkPoint> storage(numGlyphs);
    SkPoint* pos = storage.get();
    for (unsigned i = 0; i < numGlyphs; i++) {
        pos[i].set(
            SkFloatToScalar(point.x() + verticalBaselineXOffset + glyphBuffer.xOffsetAt(from + i)),
            SkFloatToScalar(point.y() + glyphBuffer.yOffsetAt(from + i)));
    }

    paintGlyphs(gc, font, glyphBuffer.glyphs(from), numGlyphs, pos, textRect);
}

void Font::drawTextBlob(GraphicsContext* gc, const SkTextBlob* blob, const SkPoint& origin) const
{
    ASSERT(RuntimeEnabledFeatures::textBlobEnabled());

    TextDrawingModeFlags textMode = gc->textDrawingMode();
    if (textMode & TextModeFill)
        gc->drawTextBlob(blob, origin, gc->fillPaint());

    if ((textMode & TextModeStroke) && gc->hasStroke()) {
        SkPaint paint = gc->strokePaint();
        if (textMode & TextModeFill)
            paint.setLooper(0);
        gc->drawTextBlob(blob, origin, paint);
    }
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, IntRectOutsets* glyphBounds) const
{
    FloatRect bounds;
    HarfBuzzShaper shaper(this, run, nullptr, fallbackFonts, glyphBounds ? &bounds : 0);
    if (!shaper.shape())
        return 0;

    glyphBounds->setTop(ceilf(-bounds.y()));
    glyphBounds->setBottom(ceilf(bounds.maxY()));
    glyphBounds->setLeft(std::max<int>(0, ceilf(-bounds.x())));
    glyphBounds->setRight(std::max<int>(0, ceilf(bounds.maxX() - shaper.totalWidth())));

    return shaper.totalWidth();
}

// Return the code point index for the given |x| offset into the text run.
int Font::offsetForPositionForComplexText(const TextRun& run, float xFloat,
    bool includePartialGlyphs) const
{
    HarfBuzzShaper shaper(this, run);
    if (!shaper.shape())
        return 0;
    return shaper.offsetForPosition(xFloat);
}

// Return the rectangle for selecting the given range of code-points in the TextRun.
FloatRect Font::selectionRectForComplexText(const TextRun& run,
    const FloatPoint& point, int height, int from, int to) const
{
    HarfBuzzShaper shaper(this, run);
    if (!shaper.shape())
        return FloatRect();
    return shaper.selectionRect(point, height, from, to);
}

void Font::drawGlyphBuffer(GraphicsContext* context,
    const TextRunPaintInfo& runInfo, const GlyphBuffer& glyphBuffer,
    const FloatPoint& point) const
{
    if (glyphBuffer.isEmpty())
        return;

    if (RuntimeEnabledFeatures::textBlobEnabled()) {
        // Enabling text-blobs forces the blob rendering path even for uncacheable blobs.
        TextBlobPtr uncacheableTextBlob;
        TextBlobPtr& textBlob = runInfo.cachedTextBlob ? *runInfo.cachedTextBlob : uncacheableTextBlob;

        textBlob = buildTextBlob(glyphBuffer);
        if (textBlob) {
            drawTextBlob(context, textBlob.get(), point.data());
            return;
        }
    }

    // Draw each contiguous run of glyphs that use the same font data.
    const SimpleFontData* fontData = glyphBuffer.fontDataAt(0);
    unsigned lastFrom = 0;
    unsigned nextGlyph;

    for (nextGlyph = 0; nextGlyph < glyphBuffer.size(); ++nextGlyph) {
        const SimpleFontData* nextFontData = glyphBuffer.fontDataAt(nextGlyph);

        if (nextFontData != fontData) {
            drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, point,
                runInfo.bounds);

            lastFrom = nextGlyph;
            fontData = nextFontData;
        }
    }

    drawGlyphs(context, fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, point,
        runInfo.bounds);
}

float Font::floatWidthForSimpleText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, IntRectOutsets* glyphBounds) const
{
    FloatRect bounds;
    SimpleShaper shaper(this, run, nullptr, fallbackFonts, glyphBounds ? &bounds : 0);
    shaper.advance(run.length());
    float runWidth = shaper.runWidthSoFar();

    if (glyphBounds) {
        glyphBounds->setTop(ceilf(-bounds.y()));
        glyphBounds->setBottom(ceilf(bounds.maxY()));
        glyphBounds->setLeft(std::max<int>(0, ceilf(-bounds.x())));
        glyphBounds->setRight(std::max<int>(0, ceilf(bounds.maxX() - runWidth)));
    }
    return runWidth;
}

FloatRect Font::selectionRectForSimpleText(const TextRun& run, const FloatPoint& point, int h, int from, int to, bool accountForGlyphBounds) const
{
    FloatRect bounds;
    SimpleShaper shaper(this, run, nullptr, nullptr, accountForGlyphBounds ? &bounds : nullptr);
    shaper.advance(from);
    float fromX = shaper.runWidthSoFar();
    shaper.advance(to);
    float toX = shaper.runWidthSoFar();

    if (run.rtl()) {
        shaper.advance(run.length());
        float totalWidth = shaper.runWidthSoFar();
        float beforeWidth = fromX;
        float afterWidth = toX;
        fromX = totalWidth - afterWidth;
        toX = totalWidth - beforeWidth;
    }

    return FloatRect(point.x() + fromX,
        accountForGlyphBounds ? bounds.y(): point.y(),
        toX - fromX,
        accountForGlyphBounds ? bounds.maxY()- bounds.y(): h);
}

int Font::offsetForPositionForSimpleText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    float delta = x;

    SimpleShaper shaper(this, run);
    unsigned offset;
    if (run.rtl()) {
        delta -= floatWidthForSimpleText(run);
        while (1) {
            offset = shaper.currentOffset();
            float w;
            if (!shaper.advanceOneCharacter(w))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = shaper.currentOffset();
            float w;
            if (!shaper.advanceOneCharacter(w))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset;
}

} // namespace blink
