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
#include "platform/fonts/shaping/ShapeResult.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/text/TextBreakIterator.h"

namespace blink {

float ShapeResult::RunInfo::xPositionForVisualOffset(unsigned offset) const
{
    ASSERT(offset < m_numCharacters);
    if (rtl())
        offset = m_numCharacters - offset - 1;
    return xPositionForOffset(offset);
}

float ShapeResult::RunInfo::xPositionForOffset(unsigned offset) const
{
    ASSERT(offset <= m_numCharacters);
    unsigned glyphIndex = 0;
    float position = 0;
    if (rtl()) {
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
        return rtl() ? m_numCharacters : 0;

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
            return rtl() ? prevCharacterIndex : m_glyphData[glyphIndex].characterIndex;
        currentX = nextX;
        ++glyphIndex;
    }

    return rtl() ? 0 : m_numCharacters;
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
        if (direction == RTL) {
            // Convert logical offsets to visual offsets, because results are in
            // logical order while runs are in visual order.
            if (!foundFromX && from >= 0 && static_cast<unsigned>(from) < result->numCharacters())
                from = result->numCharacters() - from - 1;
            if (!foundToX && to >= 0 && static_cast<unsigned>(to) < result->numCharacters())
                to = result->numCharacters() - to - 1;
            currentX -= result->width();
        }
        for (unsigned i = 0; i < result->m_runs.size(); i++) {
            if (!result->m_runs[i])
                continue;
            ASSERT((direction == RTL) == result->m_runs[i]->rtl());
            int numCharacters = result->m_runs[i]->m_numCharacters;
            if (!foundFromX && from >= 0 && from < numCharacters) {
                fromX = result->m_runs[i]->xPositionForVisualOffset(from) + currentX;
                foundFromX = true;
            } else {
                from -= numCharacters;
            }

            if (!foundToX && to >= 0 && to < numCharacters) {
                toX = result->m_runs[i]->xPositionForVisualOffset(to) + currentX;
                foundToX = true;
            } else {
                to -= numCharacters;
            }

            if (foundFromX && foundToX)
                break;
            currentX += result->m_runs[i]->m_width;
        }
        if (direction == RTL)
            currentX -= result->width();
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

} // namespace blink
