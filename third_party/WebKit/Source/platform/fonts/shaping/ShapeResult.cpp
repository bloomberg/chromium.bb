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

#include "platform/fonts/shaping/ShapeResult.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"

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
    const unsigned numGlyphs = m_glyphData.size();
    unsigned glyphIndex = 0;
    float position = 0;
    if (rtl()) {
        while (glyphIndex < numGlyphs && m_glyphData[glyphIndex].characterIndex > offset) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        // For RTL, we need to return the right side boundary of the character.
        // Add advance of glyphs which are part of the character.
        while (glyphIndex < numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
        position += m_glyphData[glyphIndex].advance;
    } else {
        while (glyphIndex < numGlyphs && m_glyphData[glyphIndex].characterIndex < offset) {
            position += m_glyphData[glyphIndex].advance;
            ++glyphIndex;
        }
    }
    return position;
}

int ShapeResult::RunInfo::characterIndexForXPosition(float targetX) const
{
    ASSERT(targetX <= m_width);
    const unsigned numGlyphs = m_glyphData.size();
    float currentX = 0;
    float currentAdvance = m_glyphData[0].advance;
    unsigned glyphIndex = 0;

    // Sum up advances that belong to the first character.
    while (glyphIndex < numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex)
        currentAdvance += m_glyphData[++glyphIndex].advance;
    currentAdvance = currentAdvance / 2.0;
    if (targetX <= currentAdvance)
        return rtl() ? m_numCharacters : 0;

    currentX = currentAdvance;
    ++glyphIndex;
    while (glyphIndex < numGlyphs) {
        unsigned prevCharacterIndex = m_glyphData[glyphIndex - 1].characterIndex;
        float prevAdvance = currentAdvance;
        currentAdvance = m_glyphData[glyphIndex].advance;
        while (glyphIndex < numGlyphs - 1 && m_glyphData[glyphIndex].characterIndex == m_glyphData[glyphIndex + 1].characterIndex)
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
    , m_hasVerticalOffsets(0)
{
}

ShapeResult::~ShapeResult()
{
}

size_t ShapeResult::byteSize()
{
    size_t selfByteSize = sizeof(this);
    for (unsigned i = 0; i < m_runs.size(); ++i) {
        selfByteSize += m_runs[i]->byteSize();
    }
    return selfByteSize;
}

int ShapeResult::offsetForPosition(float targetX) const
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
