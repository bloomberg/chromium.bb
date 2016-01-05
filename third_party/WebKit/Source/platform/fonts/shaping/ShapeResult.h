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

#ifndef ShapeResult_h
#define ShapeResult_h

#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextRun.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace blink {

class Font;
class GlyphBuffer;
class ShapeResultBuffer;
class SimpleFontData;
class HarfBuzzShaper;
struct GlyphData;

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
    WTF_MAKE_NONCOPYABLE(ShapeResult);
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
    bool hasVerticalOffsets() const { return m_hasVerticalOffsets; }

    // TODO(fmalita): relocate these to ShapeResultBuffer
    static int offsetForPosition(const ShapeResultBuffer&,
        const TextRun&, float targetX);
    static float fillGlyphBuffer(const ShapeResultBuffer&,
        GlyphBuffer*, const TextRun&, unsigned from, unsigned to);
    static float fillGlyphBufferForTextEmphasis(const ShapeResultBuffer&,
        GlyphBuffer*, const TextRun&, const GlyphData* emphasisData,
        unsigned from, unsigned to);
    static FloatRect selectionRect(const ShapeResultBuffer&,
        TextDirection, float totalWidth, const FloatPoint&, int height,
        unsigned from, unsigned to);

    // For memory reporting.
    size_t byteSize();

protected:
    struct RunInfo;
#if COMPILER(MSVC)
    friend struct ::WTF::OwnedPtrDeleter<RunInfo>;
#endif

    ShapeResult(const Font*, unsigned numCharacters, TextDirection);

    // TODO(fmalita): relocate these to ShapeResultBuffer
    int offsetForPosition(float targetX);
    template<TextDirection>
    static float fillGlyphBufferForRun(GlyphBuffer*, const RunInfo*,
        float initialAdvance, unsigned from, unsigned to, unsigned runOffset);

    static float fillGlyphBufferForTextEmphasisRun(GlyphBuffer*, const RunInfo*,
        const TextRun&, const GlyphData*, float initialAdvance,
        unsigned from, unsigned to, unsigned runOffset);

    static float fillFastHorizontalGlyphBuffer(const ShapeResultBuffer&,
        GlyphBuffer*, TextDirection);

    float m_width;
    FloatRect m_glyphBoundingBox;
    Vector<OwnPtr<RunInfo>> m_runs;
    RefPtr<SimpleFontData> m_primaryFont;

    unsigned m_numCharacters;
    unsigned m_numGlyphs : 30;

    // Overall direction for the TextRun, dictates which order each individual
    // sub run (represented by RunInfo structs in the m_runs vector) can have a
    // different text direction.
    unsigned m_direction : 1;

    // Tracks whether any runs contain glyphs with a y-offset != 0.
    unsigned m_hasVerticalOffsets : 1;

    friend class HarfBuzzShaper;
};

class ShapeResultBuffer {
    WTF_MAKE_NONCOPYABLE(ShapeResultBuffer);
    STACK_ALLOCATED();
public:
    ShapeResultBuffer()
        : m_hasVerticalOffsets(false) { }

    void appendResult(PassRefPtr<ShapeResult> result)
    {
        m_hasVerticalOffsets |= result->hasVerticalOffsets();
        m_results.append(result);
    }

    bool hasVerticalOffsets() const { return m_hasVerticalOffsets; }

    // Empirically, cases where we get more than 50 ShapeResults are extremely rare.
    typedef Vector<RefPtr<ShapeResult>, 64> ShapeResultVector;
    const ShapeResultVector& results() const { return m_results; }

private:
    ShapeResultVector m_results;
    bool m_hasVerticalOffsets;
};

} // namespace blink

#endif // ShapeResult_h
