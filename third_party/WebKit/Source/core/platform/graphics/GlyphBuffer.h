/*
 * Copyright (C) 2006, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GlyphBuffer_h
#define GlyphBuffer_h

#include "core/platform/graphics/FloatSize.h"
#include "core/platform/graphics/Glyph.h"
#include "wtf/UnusedParam.h"
#include "wtf/Vector.h"

#if OS(MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace WebCore {

class SimpleFontData;

typedef Glyph GlyphBufferGlyph;

// CG uses CGSize instead of FloatSize so that the result of advances()
// can be passed directly to CGContextShowGlyphsWithAdvances in FontMac.mm
#if OS(MACOSX)
struct GlyphBufferAdvance : CGSize {
public:
    GlyphBufferAdvance(CGSize size) : CGSize(size)
    {
    }

    void setWidth(CGFloat width) { this->CGSize::width = width; }
    CGFloat width() const { return this->CGSize::width; }
    CGFloat height() const { return this->CGSize::height; }
};
#else
typedef FloatSize GlyphBufferAdvance;
#endif

class GlyphBuffer {
public:
    bool isEmpty() const { return m_fontData.isEmpty(); }
    int size() const { return m_fontData.size(); }

    void clear()
    {
        m_fontData.clear();
        m_glyphs.clear();
        m_advances.clear();
    }

    GlyphBufferGlyph* glyphs(int from) { return m_glyphs.data() + from; }
    GlyphBufferAdvance* advances(int from) { return m_advances.data() + from; }
    const GlyphBufferGlyph* glyphs(int from) const { return m_glyphs.data() + from; }
    const GlyphBufferAdvance* advances(int from) const { return m_advances.data() + from; }

    const SimpleFontData* fontDataAt(int index) const { return m_fontData[index]; }

    Glyph glyphAt(int index) const
    {
        return m_glyphs[index];
    }

    float advanceAt(int index) const
    {
        return m_advances[index].width();
    }

    void add(Glyph glyph, const SimpleFontData* font, float width)
    {
        m_fontData.append(font);
        m_glyphs.append(glyph);

#if OS(MACOSX)
        CGSize advance = { width, 0 };
        m_advances.append(advance);
#else
        m_advances.append(FloatSize(width, 0));
#endif
    }

    void add(Glyph glyph, const SimpleFontData* font, GlyphBufferAdvance advance)
    {
        m_fontData.append(font);
        m_glyphs.append(glyph);
        m_advances.append(advance);
    }

    void reverse(int from, int length)
    {
        for (int i = from, end = from + length - 1; i < end; ++i, --end)
            swap(i, end);
    }

    void expandLastAdvance(float width)
    {
        ASSERT(!isEmpty());
        GlyphBufferAdvance& lastAdvance = m_advances.last();
        lastAdvance.setWidth(lastAdvance.width() + width);
    }

private:
    void swap(int index1, int index2)
    {
        const SimpleFontData* f = m_fontData[index1];
        m_fontData[index1] = m_fontData[index2];
        m_fontData[index2] = f;

        GlyphBufferGlyph g = m_glyphs[index1];
        m_glyphs[index1] = m_glyphs[index2];
        m_glyphs[index2] = g;

        GlyphBufferAdvance s = m_advances[index1];
        m_advances[index1] = m_advances[index2];
        m_advances[index2] = s;
    }

    Vector<const SimpleFontData*, 2048> m_fontData;
    Vector<GlyphBufferGlyph, 2048> m_glyphs;
    Vector<GlyphBufferAdvance, 2048> m_advances;
};

}
#endif
