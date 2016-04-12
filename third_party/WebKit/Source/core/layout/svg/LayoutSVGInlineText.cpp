/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#include "core/layout/svg/LayoutSVGInlineText.h"

#include "core/css/CSSFontSelector.h"
#include "core/css/FontSize.h"
#include "core/dom/StyleEngine.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/text/BidiCharacterRun.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/TextDirection.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"

namespace blink {

static PassRefPtr<StringImpl> applySVGWhitespaceRules(PassRefPtr<StringImpl> string, bool preserveWhiteSpace)
{
    if (preserveWhiteSpace) {
        // Spec: When xml:space="preserve", the SVG user agent will do the following using a
        // copy of the original character data content. It will convert all newline and tab
        // characters into space characters. Then, it will draw all space characters, including
        // leading, trailing and multiple contiguous space characters.
        RefPtr<StringImpl> newString = string->replace('\t', ' ');
        newString = newString->replace('\n', ' ');
        newString = newString->replace('\r', ' ');
        return newString.release();
    }

    // Spec: When xml:space="default", the SVG user agent will do the following using a
    // copy of the original character data content. First, it will remove all newline
    // characters. Then it will convert all tab characters into space characters.
    // Then, it will strip off all leading and trailing space characters.
    // Then, all contiguous space characters will be consolidated.
    RefPtr<StringImpl> newString = string->replace('\n', StringImpl::empty());
    newString = newString->replace('\r', StringImpl::empty());
    newString = newString->replace('\t', ' ');
    return newString.release();
}

LayoutSVGInlineText::LayoutSVGInlineText(Node* n, PassRefPtr<StringImpl> string)
    : LayoutText(n, applySVGWhitespaceRules(string, false))
    , m_scalingFactor(1)
    , m_layoutAttributes(this)
{
}

void LayoutSVGInlineText::setTextInternal(PassRefPtr<StringImpl> text)
{
    LayoutText::setTextInternal(text);
    if (LayoutSVGText* textLayoutObject = LayoutSVGText::locateLayoutSVGTextAncestor(this))
        textLayoutObject->subtreeTextDidChange();
}

void LayoutSVGInlineText::styleDidChange(StyleDifference diff, const ComputedStyle* oldStyle)
{
    LayoutText::styleDidChange(diff, oldStyle);
    updateScaledFont();

    bool newPreserves = style() ? style()->whiteSpace() == PRE : false;
    bool oldPreserves = oldStyle ? oldStyle->whiteSpace() == PRE : false;
    if (oldPreserves != newPreserves) {
        setText(originalText(), true);
        return;
    }

    if (!diff.needsFullLayout())
        return;

    // The text metrics may be influenced by style changes.
    if (LayoutSVGText* textLayoutObject = LayoutSVGText::locateLayoutSVGTextAncestor(this)) {
        textLayoutObject->setNeedsTextMetricsUpdate();
        textLayoutObject->setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::StyleChange);
    }
}

InlineTextBox* LayoutSVGInlineText::createTextBox(int start, unsigned short length)
{
    InlineTextBox* box = new SVGInlineTextBox(LineLayoutItem(this), start, length);
    box->setHasVirtualLogicalHeight();
    return box;
}

LayoutRect LayoutSVGInlineText::localCaretRect(InlineBox* box, int caretOffset, LayoutUnit*)
{
    if (!box || !box->isInlineTextBox())
        return LayoutRect();

    InlineTextBox* textBox = toInlineTextBox(box);
    if (static_cast<unsigned>(caretOffset) < textBox->start() || static_cast<unsigned>(caretOffset) > textBox->start() + textBox->len())
        return LayoutRect();

    // Use the edge of the selection rect to determine the caret rect.
    if (static_cast<unsigned>(caretOffset) < textBox->start() + textBox->len()) {
        LayoutRect rect = textBox->localSelectionRect(caretOffset, caretOffset + 1);
        LayoutUnit x = box->isLeftToRightDirection() ? rect.x() : rect.maxX();
        return LayoutRect(x, rect.y(), caretWidth(), rect.height());
    }

    LayoutRect rect = textBox->localSelectionRect(caretOffset - 1, caretOffset);
    LayoutUnit x = box->isLeftToRightDirection() ? rect.maxX() : rect.x();
    return LayoutRect(x, rect.y(), caretWidth(), rect.height());
}

FloatRect LayoutSVGInlineText::floatLinesBoundingBox() const
{
    FloatRect boundingBox;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        boundingBox.unite(FloatRect(box->calculateBoundaries()));
    return boundingBox;
}

IntRect LayoutSVGInlineText::linesBoundingBox() const
{
    return enclosingIntRect(floatLinesBoundingBox());
}

bool LayoutSVGInlineText::characterStartsNewTextChunk(int position) const
{
    ASSERT(position >= 0);
    ASSERT(position < static_cast<int>(textLength()));

    // Each <textPath> element starts a new text chunk, regardless of any x/y values.
    if (!position && parent()->isSVGTextPath() && !previousSibling())
        return true;

    const SVGCharacterDataMap::const_iterator it = m_layoutAttributes.characterDataMap().find(static_cast<unsigned>(position + 1));
    if (it == m_layoutAttributes.characterDataMap().end())
        return false;

    return !SVGTextLayoutAttributes::isEmptyValue(it->value.x) || !SVGTextLayoutAttributes::isEmptyValue(it->value.y);
}

PositionWithAffinity LayoutSVGInlineText::positionForPoint(const LayoutPoint& point)
{
    if (!hasTextBoxes() || !textLength())
        return createPositionWithAffinity(0);

    ASSERT(m_scalingFactor);
    float baseline = m_scaledFont.getFontMetrics().floatAscent() / m_scalingFactor;

    LayoutBlock* containingBlock = this->containingBlock();
    ASSERT(containingBlock);

    // Map local point to absolute point, as the character origins stored in the text fragments use absolute coordinates.
    FloatPoint absolutePoint(point);
    absolutePoint.moveBy(containingBlock->location());

    float closestDistance = std::numeric_limits<float>::max();
    float closestDistancePosition = 0;
    const SVGTextFragment* closestDistanceFragment = nullptr;
    SVGInlineTextBox* closestDistanceBox = nullptr;

    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        if (!box->isSVGInlineTextBox())
            continue;

        SVGInlineTextBox* textBox = toSVGInlineTextBox(box);
        for (const SVGTextFragment& fragment : textBox->textFragments()) {
            FloatRect fragmentRect = fragment.boundingBox(baseline);

            float distance = 0;
            if (!fragmentRect.contains(absolutePoint))
                distance = fragmentRect.squaredDistanceTo(absolutePoint);

            if (distance <= closestDistance) {
                closestDistance = distance;
                closestDistanceBox = textBox;
                closestDistanceFragment = &fragment;
                closestDistancePosition = fragmentRect.x();
            }
        }
    }

    if (!closestDistanceFragment)
        return createPositionWithAffinity(0);

    int offset = closestDistanceBox->offsetForPositionInFragment(*closestDistanceFragment, LayoutUnit(absolutePoint.x() - closestDistancePosition), true);
    return createPositionWithAffinity(offset + closestDistanceBox->start(), offset > 0 ? VP_UPSTREAM_IF_POSSIBLE : TextAffinity::Downstream);
}

namespace {

inline bool characterStartsSurrogatePair(const TextRun& run, unsigned index)
{
    if (!U16_IS_LEAD(run[index]))
        return false;
    if (index + 1 >= static_cast<unsigned>(run.charactersLength()))
        return false;
    return U16_IS_TRAIL(run[index + 1]);
}

class SVGTextMetricsCalculator {
public:
    SVGTextMetricsCalculator(LayoutSVGInlineText&);
    ~SVGTextMetricsCalculator();

    bool advancePosition();
    unsigned currentPosition() const { return m_currentPosition; }

    SVGTextMetrics currentCharacterMetrics();

    // TODO(pdr): Character-based iteration is ambiguous and error-prone. It
    // should be unified under a single concept. See: https://crbug.com/593570
    bool currentCharacterStartsSurrogatePair() const
    {
        return characterStartsSurrogatePair(m_run, m_currentPosition);
    }
    bool currentCharacterIsWhiteSpace() const
    {
        return m_run[m_currentPosition] == ' ';
    }
    unsigned characterCount() const
    {
        return static_cast<unsigned>(m_run.charactersLength());
    }

private:
    void setupBidiRuns();

    static TextRun constructTextRun(LayoutSVGInlineText&, unsigned position, unsigned length, TextDirection);

    // Ensure |m_subrunRanges| is updated for the current bidi run, or the
    // complete m_run if no bidi runs are present. Returns the current position
    // in the subrun which can be used to index into |m_subrunRanges|.
    unsigned updateSubrunRangesForCurrentPosition();

    // Current character position in m_text.
    unsigned m_currentPosition;

    LayoutSVGInlineText& m_text;
    float m_fontScalingFactor;
    float m_cachedFontHeight;
    TextRun m_run;

    BidiCharacterRun* m_bidiRun;
    BidiResolver<TextRunIterator, BidiCharacterRun> m_bidiResolver;

    // Ranges for the current bidi run if present, or the entire run otherwise.
    Vector<CharacterRange> m_subrunRanges;
};

TextRun SVGTextMetricsCalculator::constructTextRun(LayoutSVGInlineText& text, unsigned position, unsigned length, TextDirection textDirection)
{
    const ComputedStyle& style = text.styleRef();

    TextRun run(static_cast<const LChar*>(nullptr) // characters, will be set below if non-zero.
        , 0 // length, will be set below if non-zero.
        , 0 // xPos, only relevant with allowTabs=true
        , 0 // padding, only relevant for justified text, not relevant for SVG
        , TextRun::AllowTrailingExpansion
        , textDirection
        , isOverride(style.unicodeBidi()) /* directionalOverride */);

    if (length) {
        if (text.is8Bit())
            run.setText(text.characters8() + position, length);
        else
            run.setText(text.characters16() + position, length);
    }

    // We handle letter & word spacing ourselves.
    run.disableSpacing();

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(text.textLength() - position);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

SVGTextMetricsCalculator::SVGTextMetricsCalculator(LayoutSVGInlineText& text)
    : m_currentPosition(0)
    , m_text(text)
    , m_fontScalingFactor(m_text.scalingFactor())
    , m_cachedFontHeight(m_text.scaledFont().getFontMetrics().floatHeight() / m_fontScalingFactor)
    , m_run(constructTextRun(m_text, 0, m_text.textLength(), m_text.styleRef().direction()))
    , m_bidiRun(nullptr)
{
    setupBidiRuns();
}

SVGTextMetricsCalculator::~SVGTextMetricsCalculator()
{
    m_bidiResolver.runs().deleteRuns();
}

void SVGTextMetricsCalculator::setupBidiRuns()
{
    BidiRunList<BidiCharacterRun>& bidiRuns = m_bidiResolver.runs();
    bool bidiOverride = isOverride(m_text.styleRef().unicodeBidi());
    BidiStatus status(LTR, bidiOverride);
    if (m_run.is8Bit() || bidiOverride) {
        WTF::Unicode::CharDirection direction = WTF::Unicode::LeftToRight;
        // If BiDi override is in effect, use the specified direction.
        if (bidiOverride && !m_text.styleRef().isLeftToRightDirection())
            direction = WTF::Unicode::RightToLeft;
        bidiRuns.addRun(new BidiCharacterRun(0, m_run.charactersLength(), status.context.get(), direction));
    } else {
        status.last = status.lastStrong = WTF::Unicode::OtherNeutral;
        m_bidiResolver.setStatus(status);
        m_bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&m_run, 0));
        const bool hardLineBreak = false;
        const bool reorderRuns = false;
        m_bidiResolver.createBidiRunsForLine(TextRunIterator(&m_run, m_run.length()), NoVisualOverride, hardLineBreak, reorderRuns);
    }
    m_bidiRun = bidiRuns.firstRun();
}

bool SVGTextMetricsCalculator::advancePosition()
{
    m_currentPosition += currentCharacterStartsSurrogatePair() ? 2 : 1;
    return m_currentPosition < characterCount();
}

// TODO(pdr): We only have per-glyph data so we need to synthesize per-grapheme
// data. E.g., if 'fi' is shaped into a single glyph, we do not know the 'i'
// position. The code below synthesizes an average glyph width when characters
// share a single position. This will incorrectly split combining diacritics.
// See: https://crbug.com/473476.
static void synthesizeGraphemeWidths(const TextRun& run, Vector<CharacterRange>& ranges)
{
    unsigned distributeCount = 0;
    for (int rangeIndex = static_cast<int>(ranges.size()) - 1; rangeIndex >= 0; --rangeIndex) {
        CharacterRange& currentRange = ranges[rangeIndex];
        if (currentRange.width() == 0) {
            distributeCount++;
        } else if (distributeCount != 0) {
            // Only count surrogate pairs as a single character.
            bool surrogatePair = characterStartsSurrogatePair(run, rangeIndex);
            if (!surrogatePair)
                distributeCount++;

            float newWidth = currentRange.width() / distributeCount;
            currentRange.end = currentRange.start + newWidth;
            float lastEndPosition = currentRange.end;
            for (unsigned distribute = 1; distribute < distributeCount; distribute++) {
                // This surrogate pair check will skip processing of the second
                // character forming the surrogate pair.
                unsigned distributeIndex = rangeIndex + distribute + (surrogatePair ? 1 : 0);
                ranges[distributeIndex].start = lastEndPosition;
                ranges[distributeIndex].end = lastEndPosition + newWidth;
                lastEndPosition = ranges[distributeIndex].end;
            }

            distributeCount = 0;
        }
    }
}

unsigned SVGTextMetricsCalculator::updateSubrunRangesForCurrentPosition()
{
    ASSERT(m_bidiRun);
    if (m_currentPosition >= static_cast<unsigned>(m_bidiRun->stop())) {
        m_bidiRun = m_bidiRun->next();
        // Ensure new subrange ranges are computed below.
        m_subrunRanges.clear();
    }
    ASSERT(m_bidiRun);
    ASSERT(static_cast<int>(m_currentPosition) < m_bidiRun->stop());

    unsigned positionInRun = m_currentPosition - m_bidiRun->start();
    if (positionInRun >= m_subrunRanges.size()) {
        TextRun subRun = constructTextRun(m_text, m_bidiRun->start(),
            m_bidiRun->stop() - m_bidiRun->start(), m_bidiRun->direction());
        m_subrunRanges = m_text.scaledFont().individualCharacterRanges(subRun);
        synthesizeGraphemeWidths(subRun, m_subrunRanges);
    }

    ASSERT(m_subrunRanges.size() && positionInRun < m_subrunRanges.size());
    return positionInRun;
}

SVGTextMetrics SVGTextMetricsCalculator::currentCharacterMetrics()
{
    unsigned currentSubrunPosition = updateSubrunRangesForCurrentPosition();
    unsigned length = currentCharacterStartsSurrogatePair() ? 2 : 1;
    float width = m_subrunRanges[currentSubrunPosition].width();
    return SVGTextMetrics(length, width / m_fontScalingFactor, m_cachedFontHeight);
}

} // namespace

void LayoutSVGInlineText::updateMetricsList(bool& lastCharacterWasWhiteSpace)
{
    m_metrics.clear();

    if (!textLength())
        return;

    // TODO(pdr): This loop is too tightly coupled to SVGTextMetricsCalculator.
    // We should refactor SVGTextMetricsCalculator to be a simple bidi run
    // iterator and move all subrun logic to a single function.
    SVGTextMetricsCalculator calculator(*this);
    bool preserveWhiteSpace = styleRef().whiteSpace() == PRE;
    do {
        bool currentCharacterIsWhiteSpace = calculator.currentCharacterIsWhiteSpace();
        if (!preserveWhiteSpace && lastCharacterWasWhiteSpace && currentCharacterIsWhiteSpace) {
            m_metrics.append(SVGTextMetrics(SVGTextMetrics::SkippedSpaceMetrics));
            ASSERT(calculator.currentCharacterMetrics().length() == 1);
            continue;
        }

        m_metrics.append(calculator.currentCharacterMetrics());

        lastCharacterWasWhiteSpace = currentCharacterIsWhiteSpace;
    } while (calculator.advancePosition());
}

void LayoutSVGInlineText::updateScaledFont()
{
    computeNewScaledFontForStyle(this, m_scalingFactor, m_scaledFont);
}

void LayoutSVGInlineText::computeNewScaledFontForStyle(LayoutObject* layoutObject, float& scalingFactor, Font& scaledFont)
{
    const ComputedStyle* style = layoutObject->style();
    ASSERT(style);
    ASSERT(layoutObject);

    // Alter font-size to the right on-screen value to avoid scaling the glyphs themselves, except when GeometricPrecision is specified.
    scalingFactor = SVGLayoutSupport::calculateScreenFontSizeScalingFactor(layoutObject);
    if (style->effectiveZoom() == 1 && (scalingFactor == 1 || !scalingFactor)) {
        scalingFactor = 1;
        scaledFont = style->font();
        return;
    }

    if (style->getFontDescription().textRendering() == GeometricPrecision)
        scalingFactor = 1;

    FontDescription fontDescription(style->getFontDescription());

    Document& document = layoutObject->document();
    // FIXME: We need to better handle the case when we compute very small fonts below (below 1pt).
    fontDescription.setComputedSize(FontSize::getComputedSizeFromSpecifiedSize(&document, scalingFactor, fontDescription.isAbsoluteSize(), fontDescription.specifiedSize(), DoNotUseSmartMinimumForFontSize));

    scaledFont = Font(fontDescription);
    scaledFont.update(document.styleEngine().fontSelector());
}

LayoutRect LayoutSVGInlineText::absoluteClippedOverflowRect() const
{
    return parent()->absoluteClippedOverflowRect();
}

FloatRect LayoutSVGInlineText::paintInvalidationRectInLocalSVGCoordinates() const
{
    return parent()->paintInvalidationRectInLocalSVGCoordinates();
}

PassRefPtr<StringImpl> LayoutSVGInlineText::originalText() const
{
    RefPtr<StringImpl> result = LayoutText::originalText();
    if (!result)
        return nullptr;
    return applySVGWhitespaceRules(result, style() && style()->whiteSpace() == PRE);
}

} // namespace blink
