/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef HarfBuzzShaper_h
#define HarfBuzzShaper_h

#include "hb.h"
#include "platform/fonts/shaping/Shaper.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/unicode/CharacterNames.h"

#include <unicode/uscript.h>

namespace blink {

class Font;
class GlyphBuffer;
class SimpleFontData;

class HarfBuzzShaper final : public Shaper {
public:
    HarfBuzzShaper(const Font*, const TextRun&, ForTextEmphasisOrNot = NotForTextEmphasis, HashSet<const SimpleFontData*>* fallbackFonts = 0, FloatRect* = 0);

    void setDrawRange(int from, int to);
    bool shape(GlyphBuffer* = 0);
    float totalWidth() { return m_totalWidth; }
    int offsetForPosition(float targetX);
    FloatRect selectionRect(const FloatPoint&, int height, int from, int to);

private:
    class HarfBuzzRun {
    public:
        HarfBuzzRun(const HarfBuzzRun&);
        ~HarfBuzzRun();

        static PassOwnPtr<HarfBuzzRun> create(const SimpleFontData* fontData, unsigned startIndex, unsigned numCharacters, hb_direction_t direction, hb_script_t script)
        {
            return adoptPtr(new HarfBuzzRun(fontData, startIndex, numCharacters, direction, script));
        }

        void applyShapeResult(hb_buffer_t*);
        void setGlyphAndPositions(unsigned index, uint16_t glyphId, float advance, float offsetX, float offsetY);
        void setWidth(float width) { m_width = width; }
        void addAdvance(unsigned index, float advance);

        int characterIndexForXPosition(float targetX);
        float xPositionForOffset(unsigned offset);

        const SimpleFontData* fontData() { return m_fontData; }
        unsigned startIndex() const { return m_startIndex; }
        unsigned numCharacters() const { return m_numCharacters; }
        unsigned numGlyphs() const { return m_numGlyphs; }
        uint16_t* glyphs() { ASSERT(m_numGlyphs); return &m_glyphs[0]; }
        float* advances() { ASSERT(m_numGlyphs); return &m_advances[0]; }
        FloatSize* offsets() { ASSERT(m_numGlyphs); return &m_offsets[0]; }
        uint16_t* glyphToCharacterIndexes()
        {
            ASSERT(m_numGlyphs);
            return &m_glyphToCharacterIndexes[0];
        }
        float width() { return m_width; }
        hb_direction_t direction() { return m_direction; }
        bool rtl() { return m_direction == HB_DIRECTION_RTL; }
        hb_script_t script() { return m_script; }

    private:
        HarfBuzzRun(const SimpleFontData*, unsigned startIndex, unsigned numCharacters, hb_direction_t, hb_script_t);

        const SimpleFontData* m_fontData;
        unsigned m_startIndex;
        size_t m_numCharacters;
        unsigned m_numGlyphs;
        hb_direction_t m_direction;
        hb_script_t m_script;
        Vector<uint16_t, 256> m_glyphs;
        Vector<float, 256> m_advances;
        Vector<uint16_t, 256> m_glyphToCharacterIndexes;
        Vector<FloatSize, 256> m_offsets;
        float m_width;
    };

    float nextExpansionPerOpportunity();
    // setPadding sets a number of pixels to be distributed across the TextRun.
    // WebKit uses this to justify text.
    void setExpansion(float);

    void setFontFeatures();

    template<typename CharType>
    bool shapeInternal(GlyphBuffer*);
    template<typename CharType>
    bool createHarfBuzzRuns();
    bool createHarfBuzzRunsForSingleCharacter();
    template<typename CharType>
    bool shapeHarfBuzzRuns();
    bool fillGlyphBuffer(GlyphBuffer*);
    float fillGlyphBufferFromHarfBuzzRun(GlyphBuffer*, HarfBuzzRun*, float initialAdvance);
    float fillGlyphBufferForTextEmphasis(GlyphBuffer*, HarfBuzzRun*, float initialAdvance);
    void setGlyphPositionsForHarfBuzzRun(HarfBuzzRun*, hb_buffer_t*, HarfBuzzRun* previousRun);
    float adjustSpacing(HarfBuzzRun*, size_t glyphIndex, unsigned currentCharacterIndex, HarfBuzzRun* previousRun, float& offsetX, float& totalAdvance);
    void addHarfBuzzRun(unsigned startCharacter, unsigned endCharacter, const SimpleFontData*, UScriptCode);

    float m_wordSpacingAdjustment; // Delta adjustment (pixels) for each word break.
    float m_letterSpacing; // Pixels to be added after each glyph.
    unsigned m_expansionOpportunityCount;

    Vector<hb_feature_t, 4> m_features;
    Vector<OwnPtr<HarfBuzzRun>, 16> m_harfBuzzRuns;

    int m_fromIndex;
    int m_toIndex;

    float m_totalWidth;

    friend struct CachedShapingResults;
};

} // namespace blink

#endif // HarfBuzzShaper_h
