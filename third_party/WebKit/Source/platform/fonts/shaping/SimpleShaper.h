/*
 * Copyright (C) 2003, 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef SimpleShaper_h
#define SimpleShaper_h

#include "platform/PlatformExport.h"
#include "platform/fonts/SVGGlyph.h"
#include "platform/text/TextRun.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/unicode/Unicode.h"

namespace blink {

class Font;
class GlyphBuffer;
class SimpleFontData;
class TextRun;
struct GlyphData;

struct PLATFORM_EXPORT SimpleShaper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    class GlyphBounds {
    public:
        GlyphBounds()
        {
            maxGlyphBoundingBoxY = std::numeric_limits<float>::min();
            minGlyphBoundingBoxY = std::numeric_limits<float>::max();
            firstGlyphOverflow = 0;
            lastGlyphOverflow = 0;
        }
        float maxGlyphBoundingBoxY;
        float minGlyphBoundingBoxY;
        float firstGlyphOverflow;
        float lastGlyphOverflow;
    };

    SimpleShaper(const Font*, const TextRun&, HashSet<const SimpleFontData*>* fallbackFonts = 0, GlyphBounds* = 0, bool forTextEmphasis = false);

    unsigned advance(unsigned to, GlyphBuffer* = 0);
    bool advanceOneCharacter(float& width);

    const TextRun& run() const { return m_run; }
    float runWidthSoFar() const { return m_runWidthSoFar; }
    unsigned currentOffset() { return m_currentCharacter; }

#if ENABLE(SVG_FONTS)
    Vector<SVGGlyph::ArabicForm>& arabicForms() { return m_arabicForms; }
#endif

private:
    const Font* m_font;
    const TextRun& m_run;
    unsigned m_currentCharacter;
    float m_runWidthSoFar;
    float m_expansion;
    float m_expansionPerOpportunity;
    bool m_isAfterExpansion;

#if ENABLE(SVG_FONTS)
    Vector<SVGGlyph::ArabicForm> m_arabicForms;
#endif
    struct CharacterData {
        UChar32 character;
        unsigned clusterLength;
        int characterOffset;
    };

    GlyphData glyphDataForCharacter(CharacterData&, bool normalizeSpace = false);
    float characterWidth(UChar32, const GlyphData&) const;
    void cacheFallbackFont(const SimpleFontData*, const SimpleFontData* primaryFont);
    float adjustSpacing(float, const CharacterData&, const SimpleFontData&, GlyphBuffer*);
    void updateGlyphBounds(const GlyphData&, float width, bool firstCharacter);

    template <typename TextIterator>
    unsigned advanceInternal(TextIterator&, GlyphBuffer*);

    HashSet<const SimpleFontData*>* m_fallbackFonts;
    GlyphBounds* m_bounds;
    bool m_forTextEmphasis : 1;
};

} // namespace blink

#endif
