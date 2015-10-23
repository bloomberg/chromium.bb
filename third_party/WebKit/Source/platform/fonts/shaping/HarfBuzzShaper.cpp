/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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
#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "hb.h"
#include "platform/LayoutUnit.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Character.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/fonts/shaping/RunSegmenter.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/Compiler.h"
#include "wtf/MathExtras.h"
#include "wtf/text/Unicode.h"

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace blink {

struct HarfBuzzRunGlyphData {
    uint16_t glyph;
    uint16_t characterIndex;
    float advance;
    FloatSize offset;
};

struct ShapeResult::RunInfo {
    RunInfo(const SimpleFontData* font, hb_direction_t dir, hb_script_t script,
        unsigned startIndex, unsigned numGlyphs, unsigned numCharacters)
        : m_fontData(const_cast<SimpleFontData*>(font)), m_direction(dir), m_script(script)
        , m_startIndex(startIndex), m_numCharacters(numCharacters)
        , m_numGlyphs(numGlyphs), m_width(0.0f)
    {
        m_glyphData.resize(m_numGlyphs);
    }

    float xPositionForOffset(unsigned) const;
    int characterIndexForXPosition(float) const;
    void setGlyphAndPositions(unsigned index, uint16_t glyphId, float advance,
        float offsetX, float offsetY);

    void addAdvance(unsigned index, float advance)
    {
        m_glyphData[index].advance += advance;
    }

    size_t glyphToCharacterIndex(size_t i) const
    {
        return m_startIndex + m_glyphData[i].characterIndex;
    }

    RefPtr<SimpleFontData> m_fontData;
    hb_direction_t m_direction;
    hb_script_t m_script;
    Vector<HarfBuzzRunGlyphData> m_glyphData;
    unsigned m_startIndex;
    unsigned m_numCharacters;
    unsigned m_numGlyphs;
    float m_width;
};

float ShapeResult::RunInfo::xPositionForOffset(unsigned offset) const
{
    ASSERT(offset <= m_numCharacters);
    unsigned glyphIndex = 0;
    float position = 0;
    if (m_direction == HB_DIRECTION_RTL) {
        while (glyphIndex < m_numGlyphs && m_glyphData[glyphIndex].characterIndex > offset) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        // For RTL, we need to return the right side boundary of the character.
        // Add advance of glyphs which are part of the character.
        while (glyphIndex < m_numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        position += m_glyphData[glyphIndex].advance;
    } else {
        while (glyphIndex < m_numGlyphs && m_glyphData[glyphIndex].characterIndex < offset) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
    }
    return position;
}

int ShapeResult::RunInfo::characterIndexForXPosition(float targetX) const
{
    ASSERT(targetX <= m_width);
    float currentX = 0;
    float currentAdvance = m_glyphData[0].advance;
    unsigned glyphIndex = 0;

    // Sum up advances that belong to the first character.
    while (glyphIndex < m_numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex)
        currentAdvance += m_glyphData[++glyphIndex].advance;
    currentAdvance = currentAdvance / 2.0;
    if (targetX <= currentAdvance)
        return m_direction == HB_DIRECTION_RTL ? m_numCharacters : 0;

    currentX = currentAdvance;
    ++glyphIndex;
    while (glyphIndex < m_numGlyphs) {
        unsigned prevCharacterIndex = m_glyphData[glyphIndex - 1].characterIndex;
        float prevAdvance = currentAdvance;
        currentAdvance = m_glyphData[glyphIndex].advance;
        while (glyphIndex < m_numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex)
            currentAdvance += m_glyphData[++glyphIndex].advance;
        currentAdvance = currentAdvance / 2.0;
        float nextX = currentX + prevAdvance + currentAdvance;
        if (currentX <= targetX && targetX <= nextX)
            return m_direction == HB_DIRECTION_RTL ? prevCharacterIndex : m_glyphData[glyphIndex].characterIndex;
        currentX = nextX;
        ++glyphIndex;
    }

    return m_direction == HB_DIRECTION_RTL ? 0 : m_numCharacters;
}

void ShapeResult::RunInfo::setGlyphAndPositions(unsigned index,
    uint16_t glyphId, float advance, float offsetX, float offsetY)
{
    HarfBuzzRunGlyphData& data = m_glyphData[index];
    data.glyph = glyphId;
    data.advance = advance;
    data.offset = FloatSize(offsetX, offsetY);
}

ShapeResult::ShapeResult(const Font* font, unsigned numCharacters, TextDirection direction)
    : m_width(0)
    , m_primaryFont(const_cast<SimpleFontData*>(font->primaryFont()))
    , m_numCharacters(numCharacters)
    , m_numGlyphs(0)
    , m_direction(direction)
{
}

ShapeResult::~ShapeResult()
{
}

static inline void addGlyphToBuffer(GlyphBuffer* glyphBuffer, float advance,
    hb_direction_t direction, const SimpleFontData* fontData,
    const HarfBuzzRunGlyphData& glyphData)
{
    FloatPoint startOffset = HB_DIRECTION_IS_HORIZONTAL(direction)
        ? FloatPoint(advance, 0)
        : FloatPoint(0, advance);
    glyphBuffer->add(glyphData.glyph, fontData, startOffset + glyphData.offset);
}

template<TextDirection direction>
float ShapeResult::fillGlyphBufferForRun(GlyphBuffer* glyphBuffer,
    const RunInfo* run, float initialAdvance, unsigned from, unsigned to,
    unsigned runOffset)
{
    if (!run)
        return 0;
    float advanceSoFar = initialAdvance;
    unsigned numGlyphs = run->m_numGlyphs;
    for (unsigned i = 0; i < numGlyphs; ++i) {
        const HarfBuzzRunGlyphData& glyphData = run->m_glyphData[i];
        uint16_t currentCharacterIndex = run->m_startIndex +
            glyphData.characterIndex + runOffset;
        if ((direction == RTL && currentCharacterIndex >= to)
            || (direction == LTR && currentCharacterIndex < from)) {
            advanceSoFar += glyphData.advance;
        } else if ((direction == RTL && currentCharacterIndex >= from)
            || (direction == LTR && currentCharacterIndex < to)) {
            addGlyphToBuffer(glyphBuffer, advanceSoFar, run->m_direction,
                run->m_fontData.get(), glyphData);
            advanceSoFar += glyphData.advance;
        }
    }
    return advanceSoFar - initialAdvance;
}

static inline unsigned countGraphemesInCluster(const UChar* str,
    unsigned strLength, uint16_t startIndex, uint16_t endIndex)
{
    if (startIndex > endIndex) {
        uint16_t tempIndex = startIndex;
        startIndex = endIndex;
        endIndex = tempIndex;
    }
    uint16_t length = endIndex - startIndex;
    ASSERT(static_cast<unsigned>(startIndex + length) <= strLength);
    TextBreakIterator* cursorPosIterator = cursorMovementIterator(&str[startIndex], length);

    int cursorPos = cursorPosIterator->current();
    int numGraphemes = -1;
    while (0 <= cursorPos) {
        cursorPos = cursorPosIterator->next();
        numGraphemes++;
    }
    return std::max(0, numGraphemes);
}

static inline void addEmphasisMark(GlyphBuffer* buffer,
    const GlyphData* emphasisData, FloatPoint glyphCenter,
    float midGlyphOffset)
{
    ASSERT(buffer);
    ASSERT(emphasisData);

    const SimpleFontData* emphasisFontData = emphasisData->fontData;
    ASSERT(emphasisFontData);

    bool isVertical = emphasisFontData->platformData().isVerticalAnyUpright()
        && emphasisFontData->verticalData();

    if (!isVertical) {
        buffer->add(emphasisData->glyph, emphasisFontData,
            midGlyphOffset - glyphCenter.x());
    } else {
        buffer->add(emphasisData->glyph, emphasisFontData,
            FloatPoint(-glyphCenter.x(), midGlyphOffset - glyphCenter.y()));
    }
}

float ShapeResult::fillGlyphBufferForTextEmphasisRun(GlyphBuffer* glyphBuffer,
    const RunInfo* run, const TextRun& textRun, const GlyphData* emphasisData,
    float initialAdvance, unsigned from, unsigned to, unsigned runOffset)
{
    if (!run)
        return 0;

    unsigned graphemesInCluster = 1;
    float clusterAdvance = 0;

    FloatPoint glyphCenter = emphasisData->fontData->
        boundsForGlyph(emphasisData->glyph).center();

    TextDirection direction = textRun.direction();

    // A "cluster" in this context means a cluster as it is used by HarfBuzz:
    // The minimal group of characters and corresponding glyphs, that cannot be broken
    // down further from a text shaping point of view.
    // A cluster can contain multiple glyphs and grapheme clusters, with mutually
    // overlapping boundaries. Below we count grapheme clusters per HarfBuzz clusters,
    // then linearly split the sum of corresponding glyph advances by the number of
    // grapheme clusters in order to find positions for emphasis mark drawing.
    uint16_t clusterStart = direction == RTL
        ? run->m_startIndex + run->m_numCharacters + runOffset
        : run->glyphToCharacterIndex(0) + runOffset;

    float advanceSoFar = initialAdvance;
    unsigned numGlyphs = run->m_numGlyphs;
    for (unsigned i = 0; i < numGlyphs; ++i) {
        const HarfBuzzRunGlyphData& glyphData = run->m_glyphData[i];
        uint16_t currentCharacterIndex = run->m_startIndex + glyphData.characterIndex + runOffset;
        bool isRunEnd = (i + 1 == numGlyphs);
        bool isClusterEnd =  isRunEnd || (run->glyphToCharacterIndex(i + 1) + runOffset != currentCharacterIndex);

        if ((direction == RTL && currentCharacterIndex >= to) || (direction != RTL && currentCharacterIndex < from)) {
            advanceSoFar += glyphData.advance;
            direction == RTL ? --clusterStart : ++clusterStart;
            continue;
        }

        clusterAdvance += glyphData.advance;

        if (textRun.is8Bit()) {
            float glyphAdvanceX = glyphData.advance;
            if (Character::canReceiveTextEmphasis(textRun[currentCharacterIndex])) {
                addEmphasisMark(glyphBuffer, emphasisData, glyphCenter, advanceSoFar + glyphAdvanceX / 2);
            }
            advanceSoFar += glyphAdvanceX;
        } else if (isClusterEnd) {
            uint16_t clusterEnd;
            if (direction == RTL)
                clusterEnd = currentCharacterIndex;
            else
                clusterEnd = isRunEnd ? run->m_startIndex + run->m_numCharacters + runOffset : run->glyphToCharacterIndex(i + 1) + runOffset;

            graphemesInCluster = countGraphemesInCluster(textRun.characters16(), textRun.charactersLength(), clusterStart, clusterEnd);
            if (!graphemesInCluster || !clusterAdvance)
                continue;

            float glyphAdvanceX = clusterAdvance / graphemesInCluster;
            for (unsigned j = 0; j < graphemesInCluster; ++j) {
                // Do not put emphasis marks on space, separator, and control characters.
                if (Character::canReceiveTextEmphasis(textRun[currentCharacterIndex]))
                    addEmphasisMark(glyphBuffer, emphasisData, glyphCenter, advanceSoFar + glyphAdvanceX / 2);
                advanceSoFar += glyphAdvanceX;
            }
            clusterStart = clusterEnd;
            clusterAdvance = 0;
        }
    }
    return advanceSoFar - initialAdvance;
}

float ShapeResult::fillGlyphBuffer(Vector<RefPtr<ShapeResult>>& results,
    GlyphBuffer* glyphBuffer, const TextRun& textRun,
    unsigned from, unsigned to)
{
    float advance = 0;
    if (textRun.rtl()) {
        unsigned wordOffset = textRun.length();
        for (unsigned j = 0; j < results.size(); j++) {
            unsigned resolvedIndex = results.size() - 1 - j;
            RefPtr<ShapeResult>& wordResult = results[resolvedIndex];
            for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
                advance += wordResult->fillGlyphBufferForRun<RTL>(glyphBuffer,
                    wordResult->m_runs[i].get(), advance, from, to,
                    wordOffset - wordResult->numCharacters());
            }
            wordOffset -= wordResult->numCharacters();
        }
    } else {
        unsigned wordOffset = 0;
        for (unsigned j = 0; j < results.size(); j++) {
            RefPtr<ShapeResult>& wordResult = results[j];
            for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
                advance += wordResult->fillGlyphBufferForRun<LTR>(glyphBuffer,
                    wordResult->m_runs[i].get(), advance, from, to, wordOffset);
            }
            wordOffset += wordResult->numCharacters();
        }
    }

    return advance;
}

float ShapeResult::fillGlyphBufferForTextEmphasis(
    Vector<RefPtr<ShapeResult>>& results, GlyphBuffer* glyphBuffer,
    const TextRun& textRun, const GlyphData* emphasisData,
    unsigned from, unsigned to)
{
    float advance = 0;
    unsigned wordOffset = textRun.rtl() ? textRun.length() : 0;
    for (unsigned j = 0; j < results.size(); j++) {
        unsigned resolvedIndex = textRun.rtl() ? results.size() - 1 - j : j;
        RefPtr<ShapeResult>& wordResult = results[resolvedIndex];
        for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
            unsigned resolvedOffset = wordOffset -
                (textRun.rtl() ? wordResult->numCharacters() : 0);
            advance += wordResult->fillGlyphBufferForTextEmphasisRun(
                glyphBuffer, wordResult->m_runs[i].get(), textRun, emphasisData,
                advance, from, to, resolvedOffset);
        }
        wordOffset += wordResult->numCharacters() * (textRun.rtl() ? -1 : 1);
    }

    return advance;
}

FloatRect ShapeResult::selectionRect(Vector<RefPtr<ShapeResult>>& results,
    TextDirection direction, float totalWidth, const FloatPoint& point,
    int height, unsigned absoluteFrom, unsigned absoluteTo)
{
    float currentX = 0;
    float fromX = 0;
    float toX = 0;
    bool foundFromX = false;
    bool foundToX = false;

    if (direction == RTL)
        currentX = totalWidth;

    // The absoluteFrom and absoluteTo arguments represent the start/end offset
    // for the entire run, from/to are continuously updated to be relative to
    // the current word (ShapeResult instance).
    int from = absoluteFrom;
    int to = absoluteTo;

    unsigned totalNumCharacters = 0;
    for (unsigned j = 0; j < results.size(); j++) {
        RefPtr<ShapeResult> result = results[j];
        for (unsigned i = 0; i < result->m_runs.size(); i++) {
            if (!result->m_runs[i])
                continue;
            if (direction == RTL)
                currentX -= result->m_runs[i]->m_width;
            int numCharacters = result->m_runs[i]->m_numCharacters;
            if (!foundFromX && from >= 0 && from < numCharacters) {
                fromX = result->m_runs[i]->xPositionForOffset(from) + currentX;
                foundFromX = true;
            } else {
                from -= numCharacters;
            }

            if (!foundToX && to >= 0 && to < numCharacters) {
                toX = result->m_runs[i]->xPositionForOffset(to) + currentX;
                foundToX = true;
            } else {
                to -= numCharacters;
            }

            if (foundFromX && foundToX)
                break;
            if (direction != RTL)
                currentX += result->m_runs[i]->m_width;
        }
        totalNumCharacters += result->numCharacters();
    }

    // The position in question might be just after the text.
    if (!foundFromX && absoluteFrom == totalNumCharacters) {
        fromX = direction == RTL ? 0 : totalWidth;
        foundFromX = true;
    }
    if (!foundToX && absoluteTo == totalNumCharacters) {
        toX = direction == RTL ? 0 : totalWidth;
        foundToX = true;
    }
    if (!foundFromX)
        fromX = 0;
    if (!foundToX)
        toX = direction == RTL ? 0 : totalWidth;

    // None of our runs is part of the selection, possibly invalid arguments.
    if (!foundToX && !foundFromX)
        fromX = toX = 0;
    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);
    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

int ShapeResult::offsetForPosition(Vector<RefPtr<ShapeResult>>& results,
    const TextRun& run, float targetX)
{
    unsigned totalOffset;
    if (run.rtl()) {
        totalOffset = run.length();
        for (unsigned i = results.size(); i; --i) {
            const RefPtr<ShapeResult>& wordResult = results[i - 1];
            if (!wordResult)
                continue;
            totalOffset -= wordResult->numCharacters();
            if (targetX >= 0 && targetX <= wordResult->width()) {
                int offsetForWord = wordResult->offsetForPosition(targetX);
                return totalOffset + offsetForWord;
            }
            targetX -= wordResult->width();
        }
    } else {
        totalOffset = 0;
        for (auto& wordResult : results) {
            if (!wordResult)
                continue;
            int offsetForWord = wordResult->offsetForPosition(targetX);
            ASSERT(offsetForWord >= 0);
            totalOffset += offsetForWord;
            if (targetX >= 0 && targetX <= wordResult->width())
                return totalOffset;
            targetX -= wordResult->width();
        }
    }
    return totalOffset;
}

int ShapeResult::offsetForPosition(float targetX)
{
    int charactersSoFar = 0;
    float currentX = 0;

    if (m_direction == RTL) {
        charactersSoFar = m_numCharacters;
        for (unsigned i = 0; i < m_runs.size(); ++i) {
            if (!m_runs[i])
                continue;
            charactersSoFar -= m_runs[i]->m_numCharacters;
            float nextX = currentX + m_runs[i]->m_width;
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_runs[i]->m_width) {
                // The x value in question is within this script run.
                const unsigned index = m_runs[i]->characterIndexForXPosition(offsetForRun);
                return charactersSoFar + index;
            }
            currentX = nextX;
        }
    } else {
        for (unsigned i = 0; i < m_runs.size(); ++i) {
            if (!m_runs[i])
                continue;
            float nextX = currentX + m_runs[i]->m_width;
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_runs[i]->m_width) {
                const unsigned index = m_runs[i]->characterIndexForXPosition(offsetForRun);
                return charactersSoFar + index;
            }
            charactersSoFar += m_runs[i]->m_numCharacters;
            currentX = nextX;
        }
    }

    return charactersSoFar;
}

void ShapeResult::fallbackFonts(HashSet<const SimpleFontData*>* fallback) const
{
    ASSERT(fallback);
    ASSERT(m_primaryFont);
    for (unsigned i = 0; i < m_runs.size(); ++i) {
        if (m_runs[i] && m_runs[i]->m_fontData != m_primaryFont
            && !m_runs[i]->m_fontData->isTextOrientationFallbackOf(m_primaryFont.get())) {
            fallback->add(m_runs[i]->m_fontData.get());
        }
    }
}

unsigned ShapeResult::numberOfRunsForTesting() const
{
    return m_runs.size();
}

bool ShapeResult::runInfoForTesting(unsigned runIndex, unsigned& startIndex,
    unsigned& numGlyphs, hb_script_t& script)
{
    if (runIndex < m_runs.size() && m_runs[runIndex]) {
        startIndex = m_runs[runIndex]->m_startIndex;
        numGlyphs = m_runs[runIndex]->m_numGlyphs;
        script = m_runs[runIndex]->m_script;
        return true;
    }
    return false;
}

uint16_t ShapeResult::glyphForTesting(unsigned runIndex, size_t glyphIndex)
{
    return m_runs[runIndex]->m_glyphData[glyphIndex].glyph;
}

float ShapeResult::advanceForTesting(unsigned runIndex, size_t glyphIndex)
{
    return m_runs[runIndex]->m_glyphData[glyphIndex].advance;
}

template<typename T>
class HarfBuzzScopedPtr {
public:
    typedef void (*DestroyFunction)(T*);

    HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
        : m_ptr(ptr)
        , m_destroy(destroy)
    {
        ASSERT(m_destroy);
    }
    ~HarfBuzzScopedPtr()
    {
        if (m_ptr)
            (*m_destroy)(m_ptr);
    }

    T* get() { return m_ptr; }
    void set(T* ptr) { m_ptr = ptr; }
private:
    T* m_ptr;
    DestroyFunction m_destroy;
};

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

static void normalizeCharacters(const TextRun& run, unsigned length, UChar* destination, unsigned* destinationLength)
{
    unsigned position = 0;
    bool error = false;
    const UChar* source;
    String stringFor8BitRun;
    if (run.is8Bit()) {
        stringFor8BitRun = String::make16BitFrom8BitSource(run.characters8(), run.length());
        source = stringFor8BitRun.characters16();
    } else {
        source = run.characters16();
    }

    *destinationLength = 0;
    while (position < length) {
        UChar32 character;
        U16_NEXT(source, position, length, character);
        // Don't normalize tabs as they are not treated as spaces for word-end.
        if (run.normalizeSpace() && Character::isNormalizedCanvasSpaceCharacter(character))
            character = spaceCharacter;
        else if (Character::treatAsSpace(character) && character != noBreakSpaceCharacter)
            character = spaceCharacter;
        else if (Character::treatAsZeroWidthSpaceInComplexScript(character))
            character = zeroWidthSpaceCharacter;

        U16_APPEND(destination, *destinationLength, length, character, error);
        ASSERT_UNUSED(error, !error);
    }
}

HarfBuzzShaper::HarfBuzzShaper(const Font* font, const TextRun& run)
    : Shaper(font, run)
    , m_normalizedBufferLength(0)
    , m_wordSpacingAdjustment(font->fontDescription().wordSpacing())
    , m_letterSpacing(font->fontDescription().letterSpacing())
    , m_expansionOpportunityCount(0)
{
    m_normalizedBuffer = adoptArrayPtr(new UChar[m_textRun.length() + 1]);
    normalizeCharacters(m_textRun, m_textRun.length(), m_normalizedBuffer.get(), &m_normalizedBufferLength);
    setExpansion(m_textRun.expansion());
    setFontFeatures();
}

float HarfBuzzShaper::nextExpansionPerOpportunity()
{
    if (!m_expansionOpportunityCount) {
        ASSERT_NOT_REACHED(); // failures indicate that the logic in HarfBuzzShaper does not match to the one in expansionOpportunityCount()
        return 0;
    }
    if (!--m_expansionOpportunityCount) {
        float remaining = m_expansion;
        m_expansion = 0;
        return remaining;
    }
    m_expansion -= m_expansionPerOpportunity;
    return m_expansionPerOpportunity;
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void HarfBuzzShaper::setExpansion(float padding)
{
    m_expansion = padding;
    if (!m_expansion)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each expansion opportunity.
    bool isAfterExpansion = m_isAfterExpansion;
    m_expansionOpportunityCount = Character::expansionOpportunityCount(m_normalizedBuffer.get(), m_normalizedBufferLength, m_textRun.direction(), isAfterExpansion, m_textRun.textJustify());
    if (isAfterExpansion && !m_textRun.allowsTrailingExpansion()) {
        ASSERT(m_expansionOpportunityCount > 0);
        --m_expansionOpportunityCount;
    }

    if (m_expansionOpportunityCount)
        m_expansionPerOpportunity = m_expansion / m_expansionOpportunityCount;
    else
        m_expansionPerOpportunity = 0;
}

static inline hb_feature_t createFeature(uint8_t c1, uint8_t c2, uint8_t c3, uint8_t c4, uint32_t value = 0)
{
    return { HB_TAG(c1, c2, c3, c4), value, 0 /* start */, static_cast<unsigned>(-1) /* end */ };
}

void HarfBuzzShaper::setFontFeatures()
{
    const FontDescription& description = m_font->fontDescription();

    static hb_feature_t noKern = createFeature('k', 'e', 'r', 'n');
    static hb_feature_t noVkrn = createFeature('v', 'k', 'r', 'n');
    switch (description.kerning()) {
    case FontDescription::NormalKerning:
        // kern/vkrn are enabled by default
        break;
    case FontDescription::NoneKerning:
        m_features.append(description.isVerticalAnyUpright() ? noVkrn : noKern);
        break;
    case FontDescription::AutoKerning:
        break;
    }

    static hb_feature_t noClig = createFeature('c', 'l', 'i', 'g');
    static hb_feature_t noLiga = createFeature('l', 'i', 'g', 'a');
    switch (description.commonLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_features.append(noLiga);
        m_features.append(noClig);
        break;
    case FontDescription::EnabledLigaturesState:
        // liga and clig are on by default
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t dlig = createFeature('d', 'l', 'i', 'g', 1);
    switch (description.discretionaryLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        // dlig is off by default
        break;
    case FontDescription::EnabledLigaturesState:
        m_features.append(dlig);
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t hlig = createFeature('h', 'l', 'i', 'g', 1);
    switch (description.historicalLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        // hlig is off by default
        break;
    case FontDescription::EnabledLigaturesState:
        m_features.append(hlig);
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t noCalt = createFeature('c', 'a', 'l', 't');
    switch (description.contextualLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_features.append(noCalt);
        break;
    case FontDescription::EnabledLigaturesState:
        // calt is on by default
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }

    static hb_feature_t hwid = createFeature('h', 'w', 'i', 'd', 1);
    static hb_feature_t twid = createFeature('t', 'w', 'i', 'd', 1);
    static hb_feature_t qwid = createFeature('q', 'w', 'i', 'd', 1);
    switch (description.widthVariant()) {
    case HalfWidth:
        m_features.append(hwid);
        break;
    case ThirdWidth:
        m_features.append(twid);
        break;
    case QuarterWidth:
        m_features.append(qwid);
        break;
    case RegularWidth:
        break;
    }

    FontFeatureSettings* settings = description.featureSettings();
    if (!settings)
        return;

    unsigned numFeatures = settings->size();
    for (unsigned i = 0; i < numFeatures; ++i) {
        hb_feature_t feature;
        const AtomicString& tag = settings->at(i).tag();
        feature.tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
        feature.value = settings->at(i).value();
        feature.start = 0;
        feature.end = static_cast<unsigned>(-1);
        m_features.append(feature);
    }
}

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built
// without hb-icu. See http://crbug.com/356929
static inline hb_script_t ICUScriptToHBScript(UScriptCode script)
{
    if (UNLIKELY(script == USCRIPT_INVALID_CODE))
        return HB_SCRIPT_INVALID;

    return hb_script_from_string(uscript_getShortName(script), -1);
}

static inline hb_direction_t TextDirectionToHBDirection(TextDirection dir, FontOrientation orientation, const SimpleFontData* fontData)
{
    hb_direction_t harfBuzzDirection = isVerticalAnyUpright(orientation) && !fontData->isTextOrientationFallback() ? HB_DIRECTION_TTB : HB_DIRECTION_LTR;
    return dir == RTL ? HB_DIRECTION_REVERSE(harfBuzzDirection) : harfBuzzDirection;
}

static const uint16_t* toUint16(const UChar* src)
{
    // FIXME: This relies on undefined behavior however it works on the
    // current versions of all compilers we care about and avoids making
    // a copy of the string.
    static_assert(sizeof(UChar) == sizeof(uint16_t), "UChar should be the same size as uint16_t");
    return reinterpret_cast<const uint16_t*>(src);
}

static inline void addToHarfBuzzBufferInternal(hb_buffer_t* buffer,
    const FontDescription& fontDescription, const UChar* normalizedBuffer,
    unsigned startIndex, unsigned numCharacters)
{
    // TODO: Revisit whether we can always fill the hb_buffer_t with the
    // full run text, but only specify startIndex and numCharacters for the part
    // to be shaped. Then simplify/change the complicated index computations in
    // extractShapeResults().
    if (fontDescription.variant() == FontVariantSmallCaps) {
        String upperText = String(normalizedBuffer + startIndex, numCharacters)
            .upper();
        // TextRun is 16 bit, therefore upperText is 16 bit, even after we call
        // makeUpper().
        ASSERT(!upperText.is8Bit());
        hb_buffer_add_utf16(buffer, toUint16(upperText.characters16()),
            numCharacters, 0, numCharacters);
    } else {
        hb_buffer_add_utf16(buffer, toUint16(normalizedBuffer + startIndex),
            numCharacters, 0, numCharacters);
    }
}

inline bool HarfBuzzShaper::shapeRange(hb_buffer_t* harfBuzzBuffer,
    unsigned startIndex,
    unsigned numCharacters,
    const SimpleFontData* currentFont,
    unsigned currentFontRangeFrom,
    unsigned currentFontRangeTo,
    UScriptCode currentRunScript,
    hb_language_t language)
{
    const FontPlatformData* platformData = &(currentFont->platformData());
    HarfBuzzFace* face = platformData->harfBuzzFace();
    if (!face) {
        WTF_LOG_ERROR("Could not create HarfBuzzFace from FontPlatformData.");
        return false;
    }

    hb_buffer_set_language(harfBuzzBuffer, language);
    hb_buffer_set_script(harfBuzzBuffer, ICUScriptToHBScript(currentRunScript));
    hb_buffer_set_direction(harfBuzzBuffer, TextDirectionToHBDirection(m_textRun.direction(),
        m_font->fontDescription().orientation(), currentFont));

    // Add a space as pre-context to the buffer. This prevents showing dotted-circle
    // for combining marks at the beginning of runs.
    static const uint16_t preContext = spaceCharacter;
    hb_buffer_add_utf16(harfBuzzBuffer, &preContext, 1, 1, 0);

    addToHarfBuzzBufferInternal(harfBuzzBuffer,
        m_font->fontDescription(), m_normalizedBuffer.get(), startIndex,
        numCharacters);

    HarfBuzzScopedPtr<hb_font_t> harfBuzzFont(face->createFont(currentFontRangeFrom, currentFontRangeTo), hb_font_destroy);
    hb_shape(harfBuzzFont.get(), harfBuzzBuffer, m_features.isEmpty() ? 0 : m_features.data(), m_features.size());

    return true;
}

bool HarfBuzzShaper::extractShapeResults(hb_buffer_t* harfBuzzBuffer,
    ShapeResult* shapeResult,
    bool& fontCycleQueued, const HolesQueueItem& currentQueueItem,
    const SimpleFontData* currentFont,
    UScriptCode currentRunScript,
    bool isLastResort)
{
    enum ClusterResult {
        Shaped,
        NotDef,
        Unknown
    };
    ClusterResult currentClusterResult = Unknown;
    ClusterResult previousClusterResult = Unknown;
    unsigned previousCluster = 0;
    unsigned currentCluster = 0;

    // Find first notdef glyph in harfBuzzBuffer.
    unsigned numGlyphs = hb_buffer_get_length(harfBuzzBuffer);
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);

    unsigned lastChangePosition = 0;

    if (!numGlyphs) {
        WTF_LOG_ERROR("HarfBuzz returned empty glyph buffer after shaping.");
        return false;
    }

    for (unsigned glyphIndex = 0; glyphIndex <= numGlyphs; ++glyphIndex) {
        // Iterating by clusters, check for when the state switches from shaped
        // to non-shaped and vice versa. Taking into account the edge cases of
        // beginning of the run and end of the run.
        previousCluster = currentCluster;
        currentCluster = glyphInfo[glyphIndex].cluster;

        if (glyphIndex < numGlyphs) {
            // Still the same cluster, merge shaping status.
            if (previousCluster == currentCluster && glyphIndex != 0) {
                if (glyphInfo[glyphIndex].codepoint == 0) {
                    currentClusterResult = NotDef;
                } else {
                    // We can only call the current cluster fully shapped, if
                    // all characters that are part of it are shaped, so update
                    // currentClusterResult to Shaped only if the previous
                    // characters have been shaped, too.
                    currentClusterResult = currentClusterResult == Shaped ? Shaped : NotDef;
                }
                continue;
            }
            // We've moved to a new cluster.
            previousClusterResult = currentClusterResult;
            currentClusterResult = glyphInfo[glyphIndex].codepoint == 0 ? NotDef : Shaped;
        } else {
            // The code below operates on the "flanks"/changes between NotDef
            // and Shaped. In order to keep the code below from explictly
            // dealing with character indices and run end, we explicitly
            // terminate the cluster/run here by setting the result value to the
            // opposite of what it was, leading to atChange turning true.
            previousClusterResult = currentClusterResult;
            currentClusterResult = currentClusterResult == NotDef ? Shaped : NotDef;
        }

        bool atChange = (previousClusterResult != currentClusterResult) && previousClusterResult != Unknown;
        if (!atChange)
            continue;

        // Compute the range indices of consecutive shaped or .notdef glyphs.
        // Cluster information for RTL runs becomes reversed, e.g. character 0
        // has cluster index 5 in a run of 6 characters.
        unsigned numCharacters = 0;
        unsigned numGlyphsToInsert = 0;
        unsigned startIndex = 0;
        if (HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfBuzzBuffer))) {
            startIndex = currentQueueItem.m_startIndex + glyphInfo[lastChangePosition].cluster;
            if (glyphIndex == numGlyphs) {
                numCharacters = currentQueueItem.m_numCharacters - glyphInfo[lastChangePosition].cluster;
                numGlyphsToInsert = numGlyphs - lastChangePosition;
            } else {
                numCharacters = glyphInfo[glyphIndex].cluster - glyphInfo[lastChangePosition].cluster;
                numGlyphsToInsert = glyphIndex - lastChangePosition;
            }
        } else {
            // Direction Backwards
            startIndex = currentQueueItem.m_startIndex + glyphInfo[glyphIndex - 1].cluster;
            if (lastChangePosition == 0) {
                numCharacters = currentQueueItem.m_numCharacters - glyphInfo[glyphIndex - 1].cluster;
            } else {
                numCharacters = glyphInfo[lastChangePosition - 1].cluster - glyphInfo[glyphIndex - 1].cluster;
            }
            numGlyphsToInsert = glyphIndex - lastChangePosition;
        }

        if (currentClusterResult == Shaped && !isLastResort) {
            // Now it's clear that we need to continue processing.
            if (!fontCycleQueued) {
                appendToHolesQueue(HolesQueueNextFont, 0, 0);
                fontCycleQueued = true;
            }

            // Here we need to put character positions.
            ASSERT(numCharacters);
            appendToHolesQueue(HolesQueueRange, startIndex, numCharacters);
        }

        // If numCharacters is 0, that means we hit a NotDef before shaping the
        // whole grapheme. We do not append it here. For the next glyph we
        // encounter, atChange will be true, and the characters corresponding to
        // the grapheme will be added to the TODO queue again, attempting to
        // shape the whole grapheme with the next font.
        // When we're getting here with the last resort font, we have no other
        // choice than adding boxes to the ShapeResult.
        if ((currentClusterResult == NotDef && numCharacters) || isLastResort) {
            // Here we need to specify glyph positions.
            OwnPtr<ShapeResult::RunInfo> run = adoptPtr(new ShapeResult::RunInfo(currentFont,
                TextDirectionToHBDirection(m_textRun.direction(),
                m_font->fontDescription().orientation(), currentFont),
                ICUScriptToHBScript(currentRunScript),
                startIndex,
                numGlyphsToInsert, numCharacters));
            insertRunIntoShapeResult(shapeResult, run.release(), lastChangePosition, numGlyphsToInsert, currentQueueItem.m_startIndex, harfBuzzBuffer);
        }
        lastChangePosition = glyphIndex;
    }
    return true;
}

static inline const SimpleFontData* fontDataAdjustedForOrientation(const SimpleFontData* originalFont,
    FontOrientation runOrientation,
    OrientationIterator::RenderOrientation renderOrientation)
{
    if (!isVerticalBaseline(runOrientation))
        return originalFont;

    if (runOrientation == FontOrientation::VerticalRotated
        || (runOrientation == FontOrientation::VerticalMixed && renderOrientation == OrientationIterator::OrientationRotateSideways))
        return originalFont->verticalRightOrientationFontData().get();

    return originalFont;
}

bool HarfBuzzShaper::collectFallbackHintChars(Vector<UChar32>& hint, bool needsList)
{
    if (!m_holesQueue.size())
        return false;

    hint.clear();

    size_t numCharsAdded = 0;
    for (auto it = m_holesQueue.begin(); it != m_holesQueue.end(); ++it) {
        if (it->m_action == HolesQueueNextFont)
            break;

        UChar32 hintChar;
        RELEASE_ASSERT(it->m_startIndex + it->m_numCharacters <= m_normalizedBufferLength);
        UTF16TextIterator iterator(m_normalizedBuffer.get() + it->m_startIndex, it->m_numCharacters);
        while (iterator.consume(hintChar)) {
            hint.append(hintChar);
            numCharsAdded++;
            if (!needsList)
                break;
            iterator.advance();
        }
    }
    return numCharsAdded > 0;
}

void HarfBuzzShaper::appendToHolesQueue(HolesQueueItemAction action,
    unsigned startIndex,
    unsigned numCharacters)
{
    m_holesQueue.append(HolesQueueItem(action, startIndex, numCharacters));
}

PassRefPtr<ShapeResult> HarfBuzzShaper::shapeResult()
{
    RefPtr<ShapeResult> result = ShapeResult::create(m_font,
        m_normalizedBufferLength, m_textRun.direction());
    HarfBuzzScopedPtr<hb_buffer_t> harfBuzzBuffer(hb_buffer_create(), hb_buffer_destroy);

    const FontDescription& fontDescription = m_font->fontDescription();
    const String& localeString = fontDescription.locale();
    CString locale = localeString.latin1();
    const hb_language_t language = hb_language_from_string(locale.data(), locale.length());

    RunSegmenter::RunSegmenterRange segmentRange = {
        0,
        0,
        USCRIPT_INVALID_CODE,
        OrientationIterator::OrientationInvalid,
        SmallCapsIterator::SmallCapsSameCase };
    RunSegmenter runSegmenter(
        m_normalizedBuffer.get(),
        m_normalizedBufferLength,
        m_font->fontDescription().orientation(),
        fontDescription.variant());

    Vector<UChar32> fallbackCharsHint;

    // TODO: Check whether this treatAsZerowidthspace from the previous script
    // segmentation plays a role here, does the new scriptRuniterator handle that correctly?
    while (runSegmenter.consume(&segmentRange)) {
        RefPtr<FontFallbackIterator> fallbackIterator = m_font->createFontFallbackIterator();

        appendToHolesQueue(HolesQueueNextFont, 0, 0);
        appendToHolesQueue(HolesQueueRange, segmentRange.start, segmentRange.end - segmentRange.start);

        const SimpleFontData* currentFont = nullptr;
        unsigned currentFontRangeFrom = 0;
        unsigned currentFontRangeTo = 0;

        bool fontCycleQueued = false;
        while (m_holesQueue.size()) {
            HolesQueueItem currentQueueItem = m_holesQueue.takeFirst();

            if (currentQueueItem.m_action == HolesQueueNextFont) {
                // For now, we're building a character list with which we probe
                // for needed fonts depending on the declared unicode-range of a
                // segmented CSS font. Alternatively, we can build a fake font
                // for the shaper and check whether any glyphs were found, or
                // define a new API on the shaper which will give us coverage
                // information?
                if (!collectFallbackHintChars(fallbackCharsHint, fallbackIterator->needsHintList())) {
                    // Give up shaping since we cannot retrieve a font fallback
                    // font without a hintlist.
                    m_holesQueue.clear();
                    break;
                }

                FontDataRange nextFontDataRange = fallbackIterator->next(fallbackCharsHint);
                currentFont = nextFontDataRange.fontData().get();
                currentFontRangeFrom = nextFontDataRange.from();
                currentFontRangeTo = nextFontDataRange.to();
                if (!currentFont) {
                    ASSERT(!m_holesQueue.size());
                    break;
                }
                fontCycleQueued = false;
                continue;
            }

            // TODO crbug.com/522964: Only use smallCapsFontData when the font does not support true smcp.  The spec
            // says: "To match the surrounding text, a font may provide alternate glyphs for caseless characters when
            // these features are enabled but when a user agent simulates small capitals, it must not attempt to
            // simulate alternates for codepoints which are considered caseless."
            const SimpleFontData* smallcapsAdjustedFont = segmentRange.smallCapsBehavior == SmallCapsIterator::SmallCapsUppercaseNeeded
                ? currentFont->smallCapsFontData(fontDescription).get()
                : currentFont;

            // Compatibility with SimpleFontData approach of keeping a flag for overriding drawing direction.
            // TODO: crbug.com/506224 This should go away in favor of storing that information elsewhere, for example in
            // ShapeResult.
            const SimpleFontData* directionAndSmallCapsAdjustedFont = fontDataAdjustedForOrientation(smallcapsAdjustedFont,
                m_font->fontDescription().orientation(),
                segmentRange.renderOrientation);

            if (!shapeRange(harfBuzzBuffer.get(),
                currentQueueItem.m_startIndex,
                currentQueueItem.m_numCharacters,
                directionAndSmallCapsAdjustedFont,
                currentFontRangeFrom,
                currentFontRangeTo,
                segmentRange.script,
                language))
                WTF_LOG_ERROR("Shaping range failed.");

            if (!extractShapeResults(harfBuzzBuffer.get(),
                result.get(),
                fontCycleQueued,
                currentQueueItem,
                directionAndSmallCapsAdjustedFont,
                segmentRange.script,
                !fallbackIterator->hasNext()))
                WTF_LOG_ERROR("Shape result extraction failed.");

            hb_buffer_reset(harfBuzzBuffer.get());
        }
    }
    return result.release();
}

// TODO crbug.com/542701: This should be a method on ShapeResult.
void HarfBuzzShaper::insertRunIntoShapeResult(ShapeResult* result,
    PassOwnPtr<ShapeResult::RunInfo> runToInsert, unsigned startGlyph, unsigned numGlyphs,
    int bufferStartCharIndex, hb_buffer_t* harfBuzzBuffer)
{
    ASSERT(numGlyphs > 0);
    OwnPtr<ShapeResult::RunInfo> run(runToInsert);

    const SimpleFontData* currentFontData = run->m_fontData.get();
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(harfBuzzBuffer, 0);

    float totalAdvance = 0.0f;
    FloatPoint glyphOrigin;
    float offsetX, offsetY;
    float* directionOffset = m_font->fontDescription().isVerticalAnyUpright() ? &offsetY : &offsetX;

    // HarfBuzz returns result in visual order, no need to flip for RTL.
    for (unsigned i = 0; i < numGlyphs; ++i) {
        bool runEnd = i + 1 == numGlyphs;
        uint16_t glyph = glyphInfos[startGlyph + i].codepoint;
        offsetX = harfBuzzPositionToFloat(glyphPositions[startGlyph + i].x_offset);
        offsetY = -harfBuzzPositionToFloat(glyphPositions[startGlyph + i].y_offset);

        // One out of x_advance and y_advance is zero, depending on
        // whether the buffer direction is horizontal or vertical.
        float advance = harfBuzzPositionToFloat(glyphPositions[startGlyph + i].x_advance - glyphPositions[startGlyph + i].y_advance);
        unsigned currentCharacterIndex = bufferStartCharIndex + glyphInfos[startGlyph + i].cluster;
        RELEASE_ASSERT(m_normalizedBufferLength > currentCharacterIndex);
        bool isClusterEnd = runEnd || glyphInfos[startGlyph + i].cluster != glyphInfos[startGlyph + i + 1].cluster;
        float spacing = 0;

        // The characterIndex of one ShapeResult run is normalized to the run's
        // startIndex and length.  TODO crbug.com/542703: Consider changing that
        // and instead pass the whole run to hb_buffer_t each time.
        run->m_glyphData.resize(numGlyphs);
        if (HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harfBuzzBuffer))) {
            run->m_glyphData[i].characterIndex = glyphInfos[startGlyph + i].cluster - glyphInfos[startGlyph].cluster;
        } else {
            run->m_glyphData[i].characterIndex = glyphInfos[startGlyph + i].cluster - glyphInfos[startGlyph + numGlyphs - 1].cluster;
        }

        if (isClusterEnd)
            spacing += adjustSpacing(run.get(), i, currentCharacterIndex, *directionOffset, totalAdvance);

        if (currentFontData->isZeroWidthSpaceGlyph(glyph)) {
            run->setGlyphAndPositions(i, glyph, 0, 0, 0);
            continue;
        }

        advance += spacing;
        if (m_textRun.rtl()) {
            // In RTL, spacing should be added to left side of glyphs.
            *directionOffset += spacing;
            if (!isClusterEnd)
                *directionOffset += m_letterSpacing;
        }

        run->setGlyphAndPositions(i, glyph, advance, offsetX, offsetY);
        totalAdvance += advance;

        FloatRect glyphBounds = currentFontData->boundsForGlyph(glyph);
        glyphBounds.move(glyphOrigin.x(), glyphOrigin.y());
        result->m_glyphBoundingBox.unite(glyphBounds);
        glyphOrigin += FloatSize(advance + offsetX, offsetY);
    }
    run->m_width = std::max(0.0f, totalAdvance);
    result->m_width += run->m_width;
    result->m_numGlyphs += numGlyphs;

    // The runs are stored in result->m_runs in visual order. For LTR, we place
    // the run to be inserted before the next run with a bigger character
    // start index. For RTL, we place the run before the next run with a lower
    // character index. Otherwise, for both directions, at the end.
    if (HB_DIRECTION_IS_FORWARD(run->m_direction)) {
        for (size_t pos = 0; pos < result->m_runs.size(); ++pos) {
            if (result->m_runs.at(pos)->m_startIndex > run->m_startIndex) {
                result->m_runs.insert(pos, run.release());
                break;
            }
        }
    } else {
        for (size_t pos = 0; pos < result->m_runs.size(); ++pos) {
            if (result->m_runs.at(pos)->m_startIndex < run->m_startIndex) {
                result->m_runs.insert(pos, run.release());
                break;
            }
        }
    }
    // If we didn't find an existing slot to place it, append.
    if (run) {
        result->m_runs.append(run.release());
    }
}

PassRefPtr<ShapeResult> ShapeResult::createForTabulationCharacters(const Font* font,
    const TextRun& textRun, float positionOffset, unsigned count)
{
    const SimpleFontData* fontData = font->primaryFont();
    OwnPtr<ShapeResult::RunInfo> run = adoptPtr(new ShapeResult::RunInfo(fontData,
        // Tab characters are always LTR or RTL, not TTB, even when isVerticalAnyUpright().
        textRun.rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR,
        HB_SCRIPT_COMMON, 0, count, count));
    float position = textRun.xPos() + positionOffset;
    float startPosition = position;
    for (unsigned i = 0; i < count; i++) {
        float advance = font->tabWidth(*fontData, textRun.tabSize(), position);
        run->m_glyphData[i].characterIndex = i;
        run->setGlyphAndPositions(i, fontData->spaceGlyph(), advance, 0, 0);
        position += advance;
    }
    run->m_width = position - startPosition;

    RefPtr<ShapeResult> result = ShapeResult::create(font, count, textRun.direction());
    result->m_width = run->m_width;
    result->m_numGlyphs = count;
    result->m_runs.append(run.release());
    return result.release();
}

float HarfBuzzShaper::adjustSpacing(ShapeResult::RunInfo* run, size_t glyphIndex, unsigned currentCharacterIndex, float& offset, float& totalAdvance)
{
    float spacing = 0;
    UChar32 character = m_normalizedBuffer[currentCharacterIndex];
    if (m_letterSpacing && !Character::treatAsZeroWidthSpace(character))
        spacing += m_letterSpacing;

    bool treatAsSpace = Character::treatAsSpace(character);
    if (treatAsSpace && (currentCharacterIndex || character == noBreakSpaceCharacter) && (character != '\t' || !m_textRun.allowTabs()))
        spacing += m_wordSpacingAdjustment;

    if (!m_expansionOpportunityCount)
        return spacing;

    if (treatAsSpace) {
        spacing += nextExpansionPerOpportunity();
        m_isAfterExpansion = true;
        return spacing;
    }

    if (m_textRun.textJustify() != TextJustify::TextJustifyAuto) {
        m_isAfterExpansion = false;
        return spacing;
    }

    // isCJKIdeographOrSymbol() has expansion opportunities both before and after each character.
    // http://www.w3.org/TR/jlreq/#line_adjustment
    if (U16_IS_LEAD(character) && currentCharacterIndex + 1 < m_normalizedBufferLength && U16_IS_TRAIL(m_normalizedBuffer[currentCharacterIndex + 1]))
        character = U16_GET_SUPPLEMENTARY(character, m_normalizedBuffer[currentCharacterIndex + 1]);
    if (!Character::isCJKIdeographOrSymbol(character)) {
        m_isAfterExpansion = false;
        return spacing;
    }

    if (!m_isAfterExpansion) {
        // Take the expansion opportunity before this ideograph.
        float expandBefore = nextExpansionPerOpportunity();
        if (expandBefore) {
            if (glyphIndex > 0) {
                run->addAdvance(glyphIndex - 1, expandBefore);
                totalAdvance += expandBefore;
            } else {
                offset += expandBefore;
                spacing += expandBefore;
            }
        }
        if (!m_expansionOpportunityCount)
            return spacing;
    }

    // Don't need to check m_textRun.allowsTrailingExpansion() since it's covered by !m_expansionOpportunityCount above
    spacing += nextExpansionPerOpportunity();
    m_isAfterExpansion = true;
    return spacing;
}


} // namespace blink
