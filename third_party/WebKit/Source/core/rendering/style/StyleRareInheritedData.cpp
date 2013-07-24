/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "core/rendering/style/StyleRareInheritedData.h"

#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/ShadowData.h"
#include "core/rendering/style/StyleImage.h"

namespace WebCore {

struct SameSizeAsStyleRareInheritedData : public RefCounted<SameSizeAsStyleRareInheritedData> {
    void* styleImage;
    StyleColor firstColor;
    float firstFloat;
    StyleColor colors[5];
    void* ownPtrs[1];
    AtomicString atomicStrings[5];
    void* refPtrs[2];
    Length lengths[1];
    float secondFloat;
    unsigned m_bitfields[2];
    short pagedMediaShorts[2];
    unsigned unsigneds[1];
    short hyphenationShorts[3];

    StyleColor touchColors;

    void* variableDataRefs[1];
};

COMPILE_ASSERT(sizeof(StyleRareInheritedData) == sizeof(SameSizeAsStyleRareInheritedData), StyleRareInheritedData_should_bit_pack);

StyleRareInheritedData::StyleRareInheritedData()
    : listStyleImage(RenderStyle::initialListStyleImage())
    , textStrokeWidth(RenderStyle::initialTextStrokeWidth())
    , indent(RenderStyle::initialTextIndent())
    , m_effectiveZoom(RenderStyle::initialZoom())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , m_hasAutoWidows(true)
    , m_hasAutoOrphans(true)
    , textSecurity(RenderStyle::initialTextSecurity())
    , userModify(READ_ONLY)
    , wordBreak(RenderStyle::initialWordBreak())
    , overflowWrap(RenderStyle::initialOverflowWrap())
    , lineBreak(LineBreakAuto)
    , resize(RenderStyle::initialResize())
    , userSelect(RenderStyle::initialUserSelect())
    , speak(SpeakNormal)
    , hyphens(HyphensManual)
    , textEmphasisFill(TextEmphasisFillFilled)
    , textEmphasisMark(TextEmphasisMarkNone)
    , textEmphasisPosition(TextEmphasisPositionOver)
    , m_textAlignLast(RenderStyle::initialTextAlignLast())
    , m_textOrientation(TextOrientationVerticalRight)
#if ENABLE(CSS3_TEXT)
    , m_textIndentLine(RenderStyle::initialTextIndentLine())
#endif
    , m_lineBoxContain(RenderStyle::initialLineBoxContain())
    , m_imageRendering(RenderStyle::initialImageRendering())
    , m_lineSnap(RenderStyle::initialLineSnap())
    , m_lineAlign(RenderStyle::initialLineAlign())
#if ENABLE(CSS3_TEXT)
    , m_textUnderlinePosition(RenderStyle::initialTextUnderlinePosition())
#endif // CSS3_TEXT
    , m_rubyPosition(RenderStyle::initialRubyPosition())
    , hyphenationLimitBefore(-1)
    , hyphenationLimitAfter(-1)
    , hyphenationLimitLines(-1)
    , m_lineGrid(RenderStyle::initialLineGrid())
    , m_tabSize(RenderStyle::initialTabSize())
    , tapHighlightColor(RenderStyle::initialTapHighlightColor())
{
    m_variables.init();
}

StyleRareInheritedData::StyleRareInheritedData(const StyleRareInheritedData& o)
    : RefCounted<StyleRareInheritedData>()
    , listStyleImage(o.listStyleImage)
    , textStrokeColor(o.textStrokeColor)
    , textStrokeWidth(o.textStrokeWidth)
    , textFillColor(o.textFillColor)
    , textEmphasisColor(o.textEmphasisColor)
    , visitedLinkTextStrokeColor(o.visitedLinkTextStrokeColor)
    , visitedLinkTextFillColor(o.visitedLinkTextFillColor)
    , visitedLinkTextEmphasisColor(o.visitedLinkTextEmphasisColor)
    , textShadow(cloneShadow(o.textShadow))
    , highlight(o.highlight)
    , cursorData(o.cursorData)
    , indent(o.indent)
    , m_effectiveZoom(o.m_effectiveZoom)
    , widows(o.widows)
    , orphans(o.orphans)
    , m_hasAutoWidows(o.m_hasAutoWidows)
    , m_hasAutoOrphans(o.m_hasAutoOrphans)
    , textSecurity(o.textSecurity)
    , userModify(o.userModify)
    , wordBreak(o.wordBreak)
    , overflowWrap(o.overflowWrap)
    , lineBreak(o.lineBreak)
    , resize(o.resize)
    , userSelect(o.userSelect)
    , speak(o.speak)
    , hyphens(o.hyphens)
    , textEmphasisFill(o.textEmphasisFill)
    , textEmphasisMark(o.textEmphasisMark)
    , textEmphasisPosition(o.textEmphasisPosition)
    , m_textAlignLast(o.m_textAlignLast)
    , m_textOrientation(o.m_textOrientation)
#if ENABLE(CSS3_TEXT)
    , m_textIndentLine(o.m_textIndentLine)
#endif
    , m_lineBoxContain(o.m_lineBoxContain)
    , m_imageRendering(o.m_imageRendering)
    , m_lineSnap(o.m_lineSnap)
    , m_lineAlign(o.m_lineAlign)
#if ENABLE(CSS3_TEXT)
    , m_textUnderlinePosition(o.m_textUnderlinePosition)
#endif // CSS3_TEXT
    , m_rubyPosition(o.m_rubyPosition)
    , hyphenationString(o.hyphenationString)
    , hyphenationLimitBefore(o.hyphenationLimitBefore)
    , hyphenationLimitAfter(o.hyphenationLimitAfter)
    , hyphenationLimitLines(o.hyphenationLimitLines)
    , locale(o.locale)
    , textEmphasisCustomMark(o.textEmphasisCustomMark)
    , m_lineGrid(o.m_lineGrid)
    , m_tabSize(o.m_tabSize)
    , tapHighlightColor(o.tapHighlightColor)
    , m_variables(o.m_variables)
{
}

StyleRareInheritedData::~StyleRareInheritedData()
{
}

static bool cursorDataEquivalent(const CursorList* c1, const CursorList* c2)
{
    if (c1 == c2)
        return true;
    if ((!c1 && c2) || (c1 && !c2))
        return false;
    return (*c1 == *c2);
}

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const
{
    return textStrokeColor == o.textStrokeColor
        && textStrokeWidth == o.textStrokeWidth
        && textFillColor == o.textFillColor
        && textEmphasisColor == o.textEmphasisColor
        && visitedLinkTextStrokeColor == o.visitedLinkTextStrokeColor
        && visitedLinkTextFillColor == o.visitedLinkTextFillColor
        && visitedLinkTextEmphasisColor == o.visitedLinkTextEmphasisColor
        && tapHighlightColor == o.tapHighlightColor
        && shadowDataEquivalent(o)
        && highlight == o.highlight
        && cursorDataEquivalent(cursorData.get(), o.cursorData.get())
        && indent == o.indent
        && m_effectiveZoom == o.m_effectiveZoom
        && widows == o.widows
        && orphans == o.orphans
        && m_hasAutoWidows == o.m_hasAutoWidows
        && m_hasAutoOrphans == o.m_hasAutoOrphans
        && textSecurity == o.textSecurity
        && userModify == o.userModify
        && wordBreak == o.wordBreak
        && overflowWrap == o.overflowWrap
        && lineBreak == o.lineBreak
        && resize == o.resize
        && userSelect == o.userSelect
        && speak == o.speak
        && hyphens == o.hyphens
        && hyphenationLimitBefore == o.hyphenationLimitBefore
        && hyphenationLimitAfter == o.hyphenationLimitAfter
        && hyphenationLimitLines == o.hyphenationLimitLines
        && textEmphasisFill == o.textEmphasisFill
        && textEmphasisMark == o.textEmphasisMark
        && textEmphasisPosition == o.textEmphasisPosition
        && m_textAlignLast == o.m_textAlignLast
        && m_textOrientation == o.m_textOrientation
#if ENABLE(CSS3_TEXT)
        && m_textIndentLine == o.m_textIndentLine
#endif
        && m_lineBoxContain == o.m_lineBoxContain
        && hyphenationString == o.hyphenationString
        && locale == o.locale
        && textEmphasisCustomMark == o.textEmphasisCustomMark
        && QuotesData::equals(quotes.get(), o.quotes.get())
        && m_tabSize == o.m_tabSize
        && m_lineGrid == o.m_lineGrid
        && m_imageRendering == o.m_imageRendering
#if ENABLE(CSS3_TEXT)
        && m_textUnderlinePosition == o.m_textUnderlinePosition
#endif // CSS3_TEXT
        && m_rubyPosition == o.m_rubyPosition
        && m_lineSnap == o.m_lineSnap
        && m_variables == o.m_variables
        && m_lineAlign == o.m_lineAlign
        && StyleImage::imagesEquivalent(listStyleImage.get(), o.listStyleImage.get());
}

bool StyleRareInheritedData::shadowDataEquivalent(const StyleRareInheritedData& o) const
{
    if ((!textShadow && o.textShadow) || (textShadow && !o.textShadow))
        return false;
    if (textShadow && o.textShadow && (*textShadow != *o.textShadow))
        return false;
    return true;
}

} // namespace WebCore
