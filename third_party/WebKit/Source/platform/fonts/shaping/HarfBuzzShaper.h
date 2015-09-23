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
#include "wtf/text/CharacterNames.h"

#include <unicode/uscript.h>

namespace blink {

class Font;
class GlyphBuffer;
class SimpleFontData;
class HarfBuzzShaper;

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
public:
    static PassRefPtr<ShapeResult> create(const Font* font,
        unsigned numCharacters, TextDirection direction)
    {
        return adoptRef(new ShapeResult(font, numCharacters, direction));
    }
    static PassRefPtr<ShapeResult> createForTabulationCharacters(const Font*,
        const TextRun&, float positionOffset, unsigned count);
    ~ShapeResult();

    float width() { return m_width; }
    FloatRect bounds() { return m_glyphBoundingBox; }
    unsigned numCharacters() const { return m_numCharacters; }
    void fallbackFonts(HashSet<const SimpleFontData*>*) const;

    static int offsetForPosition(Vector<RefPtr<ShapeResult>>&,
        const TextRun&, float targetX);
    static float fillGlyphBuffer(Vector<RefPtr<ShapeResult>>&,
        GlyphBuffer*, const TextRun&, unsigned from, unsigned to);
    static float fillGlyphBufferForTextEmphasis(Vector<RefPtr<ShapeResult>>&,
        GlyphBuffer*, const TextRun&, const GlyphData* emphasisData,
        unsigned from, unsigned to);
    static FloatRect selectionRect(Vector<RefPtr<ShapeResult>>&,
        TextDirection, float totalWidth, const FloatPoint&, int height,
        unsigned from, unsigned to);

    unsigned numberOfRunsForTesting() const;
    bool runInfoForTesting(unsigned runIndex, unsigned& startIndex,
        unsigned& numGlyphs, hb_script_t&);
    uint16_t glyphForTesting(unsigned runIndex, size_t glyphIndex);
    float advanceForTesting(unsigned runIndex, size_t glyphIndex);

private:
    struct RunInfo;

    ShapeResult(const Font*, unsigned numCharacters, TextDirection);

    int offsetForPosition(float targetX);
    template<TextDirection>
    float fillGlyphBufferForRun(GlyphBuffer*, const RunInfo*,
        float initialAdvance, unsigned from, unsigned to, unsigned runOffset);

    float fillGlyphBufferForTextEmphasisRun(GlyphBuffer*, const RunInfo*,
        const TextRun&, const GlyphData*, float initialAdvance,
        unsigned from, unsigned to, unsigned runOffset);

    float m_width;
    FloatRect m_glyphBoundingBox;
    Vector<OwnPtr<RunInfo>> m_runs;
    RefPtr<SimpleFontData> m_primaryFont;

    unsigned m_numCharacters;
    unsigned m_numGlyphs : 31;

    // Overall direction for the TextRun, dictates which order each individual
    // sub run (represented by RunInfo structs in the m_runs vector) can have a
    // different text direction.
    unsigned m_direction : 1;

    friend class HarfBuzzShaper;
};

class PLATFORM_EXPORT HarfBuzzShaper final : public Shaper {
public:
    HarfBuzzShaper(const Font*, const TextRun&);
    PassRefPtr<ShapeResult> shapeResult();
    ~HarfBuzzShaper() { }

private:
    struct HarfBuzzRun {
        const SimpleFontData* m_fontData;
        unsigned m_startIndex;
        size_t m_numCharacters;
        hb_direction_t m_direction;
        hb_script_t m_script;
    };

    float nextExpansionPerOpportunity();
    void setExpansion(float);
    void setFontFeatures();

    bool createHarfBuzzRuns();
    bool createHarfBuzzRunsForSingleCharacter();
    PassRefPtr<ShapeResult> shapeHarfBuzzRuns();
    void shapeResult(ShapeResult*, unsigned, const HarfBuzzRun*, hb_buffer_t*);
    float adjustSpacing(ShapeResult::RunInfo*, size_t glyphIndex, unsigned currentCharacterIndex, float& offsetX, float& totalAdvance);
    void addHarfBuzzRun(unsigned startCharacter, unsigned endCharacter, const SimpleFontData*, UScriptCode);

    OwnPtr<UChar[]> m_normalizedBuffer;
    unsigned m_normalizedBufferLength;

    float m_wordSpacingAdjustment; // Delta adjustment (pixels) for each word break.
    float m_letterSpacing; // Pixels to be added after each glyph.
    unsigned m_expansionOpportunityCount;

    Vector<hb_feature_t, 4> m_features;
    Vector<HarfBuzzRun, 16> m_harfBuzzRuns;
};

} // namespace blink

#endif // HarfBuzzShaper_h
