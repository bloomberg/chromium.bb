// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultSpacing_h
#define ShapeResultSpacing_h

#include "platform/PlatformExport.h"
#include "platform/text/Character.h"

namespace blink {

class FontDescription;
class ShapeResult;
class TextRun;

class PLATFORM_EXPORT ShapeResultSpacing final {
public:
    ShapeResultSpacing(const TextRun&, const FontDescription&);

    float letterSpacing() const { return m_letterSpacing; }
    bool hasSpacing() const { return m_hasSpacing; }
    bool isVerticalOffset() const { return m_isVerticalOffset; }

    float computeSpacing(const TextRun&, size_t, float& offset);

private:
    bool hasExpansion() const { return m_expansionOpportunityCount; }
    bool isAfterExpansion() const { return m_isAfterExpansion; }
    template <typename CharType>
    bool isFirstRun(const CharType*) const;

    float nextExpansion();

    template <typename CharType>
    float computeSpacing(const CharType*, size_t, size_t index, float& offset);

    const TextRun& m_textRun;
    float m_letterSpacing;
    float m_wordSpacing;
    float m_expansion;
    float m_expansionPerOpportunity;
    unsigned m_expansionOpportunityCount;
    TextJustify m_textJustify;
    bool m_hasSpacing;
    bool m_normalizeSpace;
    bool m_allowTabs;
    bool m_isAfterExpansion;
    bool m_isVerticalOffset;
};

template <>
inline bool ShapeResultSpacing::isFirstRun(const LChar* p) const
{
    return p == m_textRun.characters8();
}

template <>
inline bool ShapeResultSpacing::isFirstRun(const UChar* p) const
{
    return p == m_textRun.characters16();
}

template <typename CharType>
float ShapeResultSpacing::computeSpacing(const CharType* characters,
    size_t size, size_t index, float& offset)
{
    characters += index;
    UChar32 character = characters[0];
    bool treatAsSpace = (Character::treatAsSpace(character)
        || (m_normalizeSpace && Character::isNormalizedCanvasSpaceCharacter(character)))
        && (character != '\t' || !m_allowTabs);
    if (treatAsSpace && character != noBreakSpaceCharacter)
        character = spaceCharacter;

    float spacing = 0;
    if (m_letterSpacing && !Character::treatAsZeroWidthSpace(character))
        spacing += m_letterSpacing;

    if (treatAsSpace && (index || !isFirstRun(characters) || character == noBreakSpaceCharacter))
        spacing += m_wordSpacing;

    if (!hasExpansion())
        return spacing;

    if (treatAsSpace)
        return spacing + nextExpansion();

    if (sizeof(CharType) == sizeof(LChar)
        || m_textJustify != TextJustify::TextJustifyAuto) {
        return spacing;
    }

    // isCJKIdeographOrSymbol() has expansion opportunities both before and
    // after each character.
    // http://www.w3.org/TR/jlreq/#line_adjustment
    if (U16_IS_LEAD(character) && index + 1 < size && U16_IS_TRAIL(characters[1]))
        character = U16_GET_SUPPLEMENTARY(character, characters[1]);
    if (!Character::isCJKIdeographOrSymbol(character)) {
        m_isAfterExpansion = false;
        return spacing;
    }

    if (!m_isAfterExpansion) {
        // Take the expansion opportunity before this ideograph.
        float expandBefore = nextExpansion();
        if (expandBefore) {
            offset += expandBefore;
            spacing += expandBefore;
        }
        if (!hasExpansion())
            return spacing;
    }

    return spacing + nextExpansion();
}

} // namespace blink

#endif
