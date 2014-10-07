/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
#include "core/rendering/InlineTextBox.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/RenderedDocumentMarker.h"
#include "core/dom/Text.h"
#include "core/editing/CompositionUnderline.h"
#include "core/editing/CompositionUnderlineRangeFilter.h"
#include "core/editing/Editor.h"
#include "core/editing/InputMethodController.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "core/paint/InlineTextBoxPainter.h"
#include "core/rendering/AbstractInlineTextBox.h"
#include "core/rendering/EllipsisBox.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBR.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderCombineText.h"
#include "core/rendering/RenderRubyRun.h"
#include "core/rendering/RenderRubyText.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/TextPainter.h"
#include "core/rendering/style/ShadowList.h"
#include "core/rendering/svg/SVGTextRunRenderingContext.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/SimpleShaper.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuilder.h"

#include <algorithm>

namespace blink {

struct SameSizeAsInlineTextBox : public InlineBox {
    unsigned variables[1];
    unsigned short variables2[2];
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(InlineTextBox) == sizeof(SameSizeAsInlineTextBox), InlineTextBox_should_stay_small);

typedef WTF::HashMap<const InlineTextBox*, LayoutRect> InlineTextBoxOverflowMap;
static InlineTextBoxOverflowMap* gTextBoxesWithOverflow;

void InlineTextBox::destroy()
{
    AbstractInlineTextBox::willDestroy(this);

    if (!knownToHaveNoOverflow() && gTextBoxesWithOverflow)
        gTextBoxesWithOverflow->remove(this);
    InlineTextBoxPainter::removeFromTextBlobCache(*this);
    InlineBox::destroy();
}

void InlineTextBox::offsetRun(int delta)
{
    ASSERT(!isDirty());
    InlineTextBoxPainter::removeFromTextBlobCache(*this);
    m_start += delta;
}

void InlineTextBox::markDirty()
{
    // FIXME: Is it actually possible to try and paint a dirty InlineTextBox?
    InlineTextBoxPainter::removeFromTextBlobCache(*this);

    m_len = 0;
    m_start = 0;
    InlineBox::markDirty();
}

LayoutRect InlineTextBox::logicalOverflowRect() const
{
    if (knownToHaveNoOverflow() || !gTextBoxesWithOverflow)
        return enclosingIntRect(logicalFrameRect());
    return gTextBoxesWithOverflow->get(this);
}

void InlineTextBox::setLogicalOverflowRect(const LayoutRect& rect)
{
    ASSERT(!knownToHaveNoOverflow());
    if (!gTextBoxesWithOverflow)
        gTextBoxesWithOverflow = new InlineTextBoxOverflowMap;
    gTextBoxesWithOverflow->add(this, rect);
}

int InlineTextBox::baselinePosition(FontBaseline baselineType) const
{
    if (!isText() || !parent())
        return 0;
    if (parent()->renderer() == renderer().parent())
        return parent()->baselinePosition(baselineType);
    return toRenderBoxModelObject(renderer().parent())->baselinePosition(baselineType, isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

LayoutUnit InlineTextBox::lineHeight() const
{
    if (!isText() || !renderer().parent())
        return 0;
    if (renderer().isBR())
        return toRenderBR(renderer()).lineHeight(isFirstLineStyle());
    if (parent()->renderer() == renderer().parent())
        return parent()->lineHeight();
    return toRenderBoxModelObject(renderer().parent())->lineHeight(isFirstLineStyle(), isHorizontal() ? HorizontalLine : VerticalLine, PositionOnContainingLine);
}

bool InlineTextBox::isSelected(int startPos, int endPos) const
{
    int sPos = std::max(startPos - m_start, 0);
    // The position after a hard line break is considered to be past its end.
    // See the corresponding code in InlineTextBox::selectionState.
    int ePos = std::min(endPos - m_start, int(m_len) + (isLineBreak() ? 0 : 1));
    return (sPos < ePos);
}

RenderObject::SelectionState InlineTextBox::selectionState() const
{
    RenderObject::SelectionState state = renderer().selectionState();
    if (state == RenderObject::SelectionStart || state == RenderObject::SelectionEnd || state == RenderObject::SelectionBoth) {
        int startPos, endPos;
        renderer().selectionStartEnd(startPos, endPos);
        // The position after a hard line break is considered to be past its end.
        // See the corresponding code in InlineTextBox::isSelected.
        int lastSelectable = start() + len() - (isLineBreak() ? 1 : 0);

        // FIXME: Remove -webkit-line-break: LineBreakAfterWhiteSpace.
        int endOfLineAdjustmentForCSSLineBreak = renderer().style()->lineBreak() == LineBreakAfterWhiteSpace ? -1 : 0;
        bool start = (state != RenderObject::SelectionEnd && startPos >= m_start && startPos <= m_start + m_len + endOfLineAdjustmentForCSSLineBreak);
        bool end = (state != RenderObject::SelectionStart && endPos > m_start && endPos <= lastSelectable);
        if (start && end)
            state = RenderObject::SelectionBoth;
        else if (start)
            state = RenderObject::SelectionStart;
        else if (end)
            state = RenderObject::SelectionEnd;
        else if ((state == RenderObject::SelectionEnd || startPos < m_start) &&
                 (state == RenderObject::SelectionStart || endPos > lastSelectable))
            state = RenderObject::SelectionInside;
        else if (state == RenderObject::SelectionBoth)
            state = RenderObject::SelectionNone;
    }

    // If there are ellipsis following, make sure their selection is updated.
    if (m_truncation != cNoTruncation && root().ellipsisBox()) {
        EllipsisBox* ellipsis = root().ellipsisBox();
        if (state != RenderObject::SelectionNone) {
            int start, end;
            selectionStartEnd(start, end);
            // The ellipsis should be considered to be selected if the end of
            // the selection is past the beginning of the truncation and the
            // beginning of the selection is before or at the beginning of the
            // truncation.
            ellipsis->setSelectionState(end >= m_truncation && start <= m_truncation ?
                RenderObject::SelectionInside : RenderObject::SelectionNone);
        } else
            ellipsis->setSelectionState(RenderObject::SelectionNone);
    }

    return state;
}

LayoutRect InlineTextBox::localSelectionRect(int startPos, int endPos)
{
    int sPos = std::max(startPos - m_start, 0);
    int ePos = std::min(endPos - m_start, (int)m_len);

    if (sPos > ePos)
        return LayoutRect();

    FontCachePurgePreventer fontCachePurgePreventer;

    LayoutUnit selTop = root().selectionTop();
    LayoutUnit selHeight = root().selectionHeight();
    RenderStyle* styleToUse = renderer().style(isFirstLineStyle());
    const Font& font = styleToUse->font();

    StringBuilder charactersWithHyphen;
    bool respectHyphen = ePos == m_len && hasHyphen();
    TextRun textRun = constructTextRun(styleToUse, font, respectHyphen ? &charactersWithHyphen : 0);

    FloatPoint startingPoint = FloatPoint(logicalLeft(), selTop.toFloat());
    LayoutRect r;
    if (sPos || ePos != static_cast<int>(m_len))
        r = enclosingIntRect(font.selectionRectForText(textRun, startingPoint, selHeight, sPos, ePos));
    else // Avoid computing the font width when the entire line box is selected as an optimization.
        r = enclosingIntRect(FloatRect(startingPoint, FloatSize(m_logicalWidth, selHeight.toFloat())));

    LayoutUnit logicalWidth = r.width();
    if (r.x() > logicalRight())
        logicalWidth  = 0;
    else if (r.maxX() > logicalRight())
        logicalWidth = logicalRight() - r.x();

    LayoutPoint topPoint = isHorizontal() ? LayoutPoint(r.x(), selTop) : LayoutPoint(selTop, r.x());
    LayoutUnit width = isHorizontal() ? logicalWidth : selHeight;
    LayoutUnit height = isHorizontal() ? selHeight : logicalWidth;

    return LayoutRect(topPoint, LayoutSize(width, height));
}

void InlineTextBox::deleteLine()
{
    renderer().removeTextBox(this);
    destroy();
}

void InlineTextBox::extractLine()
{
    if (extracted())
        return;

    renderer().extractTextBox(this);
}

void InlineTextBox::attachLine()
{
    if (!extracted())
        return;

    renderer().attachTextBox(this);
}

float InlineTextBox::placeEllipsisBox(bool flowIsLTR, float visibleLeftEdge, float visibleRightEdge, float ellipsisWidth, float &truncatedWidth, bool& foundBox)
{
    if (foundBox) {
        m_truncation = cFullTruncation;
        return -1;
    }

    // For LTR this is the left edge of the box, for RTL, the right edge in parent coordinates.
    float ellipsisX = flowIsLTR ? visibleRightEdge - ellipsisWidth : visibleLeftEdge + ellipsisWidth;

    // Criteria for full truncation:
    // LTR: the left edge of the ellipsis is to the left of our text run.
    // RTL: the right edge of the ellipsis is to the right of our text run.
    bool ltrFullTruncation = flowIsLTR && ellipsisX <= logicalLeft();
    bool rtlFullTruncation = !flowIsLTR && ellipsisX >= logicalLeft() + logicalWidth();
    if (ltrFullTruncation || rtlFullTruncation) {
        // Too far.  Just set full truncation, but return -1 and let the ellipsis just be placed at the edge of the box.
        m_truncation = cFullTruncation;
        foundBox = true;
        return -1;
    }

    bool ltrEllipsisWithinBox = flowIsLTR && (ellipsisX < logicalRight());
    bool rtlEllipsisWithinBox = !flowIsLTR && (ellipsisX > logicalLeft());
    if (ltrEllipsisWithinBox || rtlEllipsisWithinBox) {
        foundBox = true;

        // The inline box may have different directionality than it's parent.  Since truncation
        // behavior depends both on both the parent and the inline block's directionality, we
        // must keep track of these separately.
        bool ltr = isLeftToRightDirection();
        if (ltr != flowIsLTR) {
            // Width in pixels of the visible portion of the box, excluding the ellipsis.
            int visibleBoxWidth = visibleRightEdge - visibleLeftEdge  - ellipsisWidth;
            ellipsisX = ltr ? logicalLeft() + visibleBoxWidth : logicalRight() - visibleBoxWidth;
        }

        int offset = offsetForPosition(ellipsisX, false);
        if (offset == 0) {
            // No characters should be rendered.  Set ourselves to full truncation and place the ellipsis at the min of our start
            // and the ellipsis edge.
            m_truncation = cFullTruncation;
            truncatedWidth += ellipsisWidth;
            return std::min(ellipsisX, logicalLeft());
        }

        // Set the truncation index on the text run.
        m_truncation = offset;

        // If we got here that means that we were only partially truncated and we need to return the pixel offset at which
        // to place the ellipsis.
        float widthOfVisibleText = renderer().width(m_start, offset, textPos(), flowIsLTR ? LTR : RTL, isFirstLineStyle());

        // The ellipsis needs to be placed just after the last visible character.
        // Where "after" is defined by the flow directionality, not the inline
        // box directionality.
        // e.g. In the case of an LTR inline box truncated in an RTL flow then we can
        // have a situation such as |Hello| -> |...He|
        truncatedWidth += widthOfVisibleText + ellipsisWidth;
        if (flowIsLTR)
            return logicalLeft() + widthOfVisibleText;
        else
            return logicalRight() - widthOfVisibleText - ellipsisWidth;
    }
    truncatedWidth += logicalWidth();
    return -1;
}

bool InlineTextBox::isLineBreak() const
{
    return renderer().isBR() || (renderer().style()->preserveNewline() && len() == 1 && (*renderer().text().impl())[start()] == '\n');
}

bool InlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    if (isLineBreak())
        return false;

    FloatPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.moveBy(accumulatedOffset);
    FloatRect rect(boxOrigin, size());
    if (m_truncation != cFullTruncation && visibleToHitTestRequest(request) && locationInContainer.intersects(rect)) {
        renderer().updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(accumulatedOffset)));
        if (!result.addNodeToRectBasedTestResult(renderer().node(), request, locationInContainer, rect))
            return true;
    }
    return false;
}

bool InlineTextBox::getEmphasisMarkPosition(RenderStyle* style, TextEmphasisPosition& emphasisPosition) const
{
    // This function returns true if there are text emphasis marks and they are suppressed by ruby text.
    if (style->textEmphasisMark() == TextEmphasisMarkNone)
        return false;

    emphasisPosition = style->textEmphasisPosition();
    if (emphasisPosition == TextEmphasisPositionUnder)
        return true; // Ruby text is always over, so it cannot suppress emphasis marks under.

    RenderBlock* containingBlock = renderer().containingBlock();
    if (!containingBlock->isRubyBase())
        return true; // This text is not inside a ruby base, so it does not have ruby text over it.

    if (!containingBlock->parent()->isRubyRun())
        return true; // Cannot get the ruby text.

    RenderRubyText* rubyText = toRenderRubyRun(containingBlock->parent())->rubyText();

    // The emphasis marks over are suppressed only if there is a ruby text box and it not empty.
    return !rubyText || !rubyText->firstLineBox();
}

void InlineTextBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /*lineTop*/, LayoutUnit /*lineBottom*/)
{
    InlineTextBoxPainter(*this).paint(paintInfo, paintOffset);
}

void InlineTextBox::selectionStartEnd(int& sPos, int& ePos) const
{
    int startPos, endPos;
    if (renderer().selectionState() == RenderObject::SelectionInside) {
        startPos = 0;
        endPos = renderer().textLength();
    } else {
        renderer().selectionStartEnd(startPos, endPos);
        if (renderer().selectionState() == RenderObject::SelectionStart)
            endPos = renderer().textLength();
        else if (renderer().selectionState() == RenderObject::SelectionEnd)
            startPos = 0;
    }

    sPos = std::max(startPos - m_start, 0);
    ePos = std::min(endPos - m_start, (int)m_len);
}

void InlineTextBox::paintDocumentMarker(GraphicsContext* pt, const FloatPoint& boxOrigin, DocumentMarker* marker, RenderStyle* style, const Font& font, bool grammar)
{
    InlineTextBoxPainter(*this).paintDocumentMarker(pt, boxOrigin, marker, style, font, grammar);
}

void InlineTextBox::paintTextMatchMarker(GraphicsContext* pt, const FloatPoint& boxOrigin, DocumentMarker* marker, RenderStyle* style, const Font& font)
{
    InlineTextBoxPainter(*this).paintTextMatchMarker(pt, boxOrigin, marker, style, font);
}

int InlineTextBox::caretMinOffset() const
{
    return m_start;
}

int InlineTextBox::caretMaxOffset() const
{
    return m_start + m_len;
}

float InlineTextBox::textPos() const
{
    // When computing the width of a text run, RenderBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPos() should be consistent so the text are rendered in the same width.
    if (logicalLeft() == 0)
        return 0;
    return logicalLeft() - root().logicalLeft();
}

int InlineTextBox::offsetForPosition(float lineOffset, bool includePartialGlyphs) const
{
    if (isLineBreak())
        return 0;

    if (lineOffset - logicalLeft() > logicalWidth())
        return isLeftToRightDirection() ? len() : 0;
    if (lineOffset - logicalLeft() < 0)
        return isLeftToRightDirection() ? 0 : len();

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderText& text = renderer();
    RenderStyle* style = text.style(isFirstLineStyle());
    const Font& font = style->font();
    return font.offsetForPosition(constructTextRun(style, font), lineOffset - logicalLeft(), includePartialGlyphs);
}

float InlineTextBox::positionForOffset(int offset) const
{
    ASSERT(offset >= m_start);
    ASSERT(offset <= m_start + m_len);

    if (isLineBreak())
        return logicalLeft();

    FontCachePurgePreventer fontCachePurgePreventer;

    RenderText& text = renderer();
    RenderStyle* styleToUse = text.style(isFirstLineStyle());
    ASSERT(styleToUse);
    const Font& font = styleToUse->font();
    int from = !isLeftToRightDirection() ? offset - m_start : 0;
    int to = !isLeftToRightDirection() ? m_len : offset - m_start;
    // FIXME: Do we need to add rightBearing here?
    return font.selectionRectForText(constructTextRun(styleToUse, font), IntPoint(logicalLeft(), 0), 0, from, to).maxX();
}

bool InlineTextBox::containsCaretOffset(int offset) const
{
    // Offsets before the box are never "in".
    if (offset < m_start)
        return false;

    int pastEnd = m_start + m_len;

    // Offsets inside the box (not at either edge) are always "in".
    if (offset < pastEnd)
        return true;

    // Offsets outside the box are always "out".
    if (offset > pastEnd)
        return false;

    // Offsets at the end are "out" for line breaks (they are on the next line).
    if (isLineBreak())
        return false;

    // Offsets at the end are "in" for normal boxes (but the caller has to check affinity).
    return true;
}

void InlineTextBox::characterWidths(Vector<float>& widths) const
{
    FontCachePurgePreventer fontCachePurgePreventer;

    RenderStyle* styleToUse = renderer().style(isFirstLineStyle());
    const Font& font = styleToUse->font();

    TextRun textRun = constructTextRun(styleToUse, font);

    SimpleShaper shaper(&font, textRun);
    float lastWidth = 0;
    widths.resize(m_len);
    for (unsigned i = 0; i < m_len; i++) {
        shaper.advance(i + 1);
        widths[i] = shaper.runWidthSoFar() - lastWidth;
        lastWidth = shaper.runWidthSoFar();
    }
}

TextRun InlineTextBox::constructTextRun(RenderStyle* style, const Font& font, StringBuilder* charactersWithHyphen) const
{
    ASSERT(style);
    ASSERT(renderer().text());

    StringView string = renderer().text().createView();
    unsigned startPos = start();
    unsigned length = len();

    if (string.length() != length || startPos)
        string.narrow(startPos, length);

    return constructTextRun(style, font, string, renderer().textLength() - startPos, charactersWithHyphen);
}

TextRun InlineTextBox::constructTextRun(RenderStyle* style, const Font& font, StringView string, int maximumLength, StringBuilder* charactersWithHyphen) const
{
    ASSERT(style);

    if (charactersWithHyphen) {
        const AtomicString& hyphenString = style->hyphenString();
        charactersWithHyphen->reserveCapacity(string.length() + hyphenString.length());
        charactersWithHyphen->append(string);
        charactersWithHyphen->append(hyphenString);
        string = charactersWithHyphen->toString().createView();
        maximumLength = string.length();
    }

    ASSERT(maximumLength >= static_cast<int>(string.length()));

    TextRun run(string, textPos(), expansion(), expansionBehavior(), direction(), dirOverride() || style->rtlOrdering() == VisualOrder, !renderer().canUseSimpleFontCodePath());
    run.setTabSize(!style->collapseWhiteSpace(), style->tabSize());
    run.setCharacterScanForCodePath(!renderer().canUseSimpleFontCodePath());
    if (textRunNeedsRenderingContext(font))
        run.setRenderingContext(SVGTextRunRenderingContext::create(&renderer()));

    // Propagate the maximum length of the characters buffer to the TextRun, even when we're only processing a substring.
    run.setCharactersLength(maximumLength);
    ASSERT(run.charactersLength() >= run.length());
    return run;
}

TextRun InlineTextBox::constructTextRunForInspector(RenderStyle* style, const Font& font) const
{
    return InlineTextBox::constructTextRun(style, font);
}

#ifndef NDEBUG

const char* InlineTextBox::boxName() const
{
    return "InlineTextBox";
}

void InlineTextBox::showBox(int printedCharacters) const
{
    const RenderText& obj = renderer();
    String value = obj.text();
    value = value.substring(start(), len());
    value.replaceWithLiteral('\\', "\\\\");
    value.replaceWithLiteral('\n', "\\n");
    printedCharacters += fprintf(stderr, "%s\t%p", boxName(), this);
    for (; printedCharacters < showTreeCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    printedCharacters = fprintf(stderr, "\t%s %p", obj.renderName(), &obj);
    const int rendererCharacterOffset = 24;
    for (; printedCharacters < rendererCharacterOffset; printedCharacters++)
        fputc(' ', stderr);
    fprintf(stderr, "(%d,%d) \"%s\"\n", start(), start() + len(), value.utf8().data());
}

#endif

} // namespace blink
