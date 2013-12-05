/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated.
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

#ifndef BreakingContextInlineHeaders_h
#define BreakingContextInlineHeaders_h

#include "core/rendering/InlineIterator.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/LineWidth.h"
#include "core/rendering/RenderCombineText.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/break_lines.h"
#include "core/rendering/line/LineInfo.h"
#include "core/rendering/shapes/ShapeInsideInfo.h"
#include "core/rendering/svg/RenderSVGInlineText.h"

namespace WebCore {

using namespace std;
using namespace WTF;
using namespace Unicode;

// We don't let our line box tree for a single line get any deeper than this.
const unsigned cMaxLineDepth = 200;

struct RenderTextInfo {
    // Destruction of m_layout requires TextLayout to be a complete type, so the constructor and destructor are made non-inline to avoid compilation errors.
    RenderTextInfo();
    ~RenderTextInfo();

    RenderText* m_text;
    OwnPtr<TextLayout> m_layout;
    LazyLineBreakIterator m_lineBreakIterator;
    const Font* m_font;

    void createLayout(RenderText* renderText, float xPos, bool collapseWhiteSpace)
    {
#if OS(MACOSX)
        m_layout = m_font->createLayoutForMacComplexText(RenderBlockFlow::constructTextRun(renderText, *m_font, renderText, renderText->style()), renderText->textLength(), xPos, collapseWhiteSpace);
#else
        m_layout = nullptr;
#endif
    }
};

class WordMeasurement {
public:
    WordMeasurement()
        : renderer(0)
        , width(0)
        , startOffset(0)
        , endOffset(0)
    {
    }

    RenderText* renderer;
    float width;
    int startOffset;
    int endOffset;
    HashSet<const SimpleFontData*> fallbackFonts;
};

// Don't call this directly. Use one of the descriptive helper functions below.
inline void deprecatedAddMidpoint(LineMidpointState& lineMidpointState, const InlineIterator& midpoint)
{
    if (lineMidpointState.midpoints.size() <= lineMidpointState.numMidpoints)
        lineMidpointState.midpoints.grow(lineMidpointState.numMidpoints + 10);

    InlineIterator* midpoints = lineMidpointState.midpoints.data();
    midpoints[lineMidpointState.numMidpoints++] = midpoint;
}

inline void startIgnoringSpaces(LineMidpointState& lineMidpointState, const InlineIterator& midpoint)
{
    ASSERT(!(lineMidpointState.numMidpoints % 2));
    deprecatedAddMidpoint(lineMidpointState, midpoint);
}

inline void stopIgnoringSpaces(LineMidpointState& lineMidpointState, const InlineIterator& midpoint)
{
    ASSERT(lineMidpointState.numMidpoints % 2);
    deprecatedAddMidpoint(lineMidpointState, midpoint);
}

// When ignoring spaces, this needs to be called for objects that need line boxes such as RenderInlines or
// hard line breaks to ensure that they're not ignored.
inline void ensureLineBoxInsideIgnoredSpaces(LineMidpointState& lineMidpointState, RenderObject* renderer)
{
    InlineIterator midpoint(0, renderer, 0);
    stopIgnoringSpaces(lineMidpointState, midpoint);
    startIgnoringSpaces(lineMidpointState, midpoint);
}

// Adding a pair of midpoints before a character will split it out into a new line box.
inline void ensureCharacterGetsLineBox(LineMidpointState& lineMidpointState, InlineIterator& textParagraphSeparator)
{
    InlineIterator midpoint(0, textParagraphSeparator.object(), textParagraphSeparator.m_pos);
    startIgnoringSpaces(lineMidpointState, InlineIterator(0, textParagraphSeparator.object(), textParagraphSeparator.m_pos - 1));
    stopIgnoringSpaces(lineMidpointState, InlineIterator(0, textParagraphSeparator.object(), textParagraphSeparator.m_pos));
}

class TrailingObjects {
public:
    TrailingObjects();
    void setTrailingWhitespace(RenderText*);
    void clear();
    void appendBoxIfNeeded(RenderBox*);

    enum CollapseFirstSpaceOrNot { DoNotCollapseFirstSpace, CollapseFirstSpace };

    void updateMidpointsForTrailingBoxes(LineMidpointState&, const InlineIterator& lBreak, CollapseFirstSpaceOrNot);

private:
    RenderText* m_whitespace;
    Vector<RenderBox*, 4> m_boxes;
};

TrailingObjects::TrailingObjects()
    : m_whitespace(0)
{
}

inline void TrailingObjects::setTrailingWhitespace(RenderText* whitespace)
{
    ASSERT(whitespace);
    m_whitespace = whitespace;
}

inline void TrailingObjects::clear()
{
    m_whitespace = 0;
    // Using resize(0) rather than clear() here saves 2% on
    // PerformanceTests/Layout/line-layout.html because we avoid freeing and
    // re-allocating the underlying buffer repeatedly.
    m_boxes.resize(0);
}

inline void TrailingObjects::appendBoxIfNeeded(RenderBox* box)
{
    if (m_whitespace)
        m_boxes.append(box);
}

void TrailingObjects::updateMidpointsForTrailingBoxes(LineMidpointState& lineMidpointState, const InlineIterator& lBreak, CollapseFirstSpaceOrNot collapseFirstSpace)
{
    if (!m_whitespace)
        return;

    // This object is either going to be part of the last midpoint, or it is going to be the actual endpoint.
    // In both cases we just decrease our pos by 1 level to exclude the space, allowing it to - in effect - collapse into the newline.
    if (lineMidpointState.numMidpoints % 2) {
        // Find the trailing space object's midpoint.
        int trailingSpaceMidpoint = lineMidpointState.numMidpoints - 1;
        for ( ; trailingSpaceMidpoint > 0 && lineMidpointState.midpoints[trailingSpaceMidpoint].object() != m_whitespace; --trailingSpaceMidpoint) { }
        ASSERT(trailingSpaceMidpoint >= 0);
        if (collapseFirstSpace == CollapseFirstSpace)
            lineMidpointState.midpoints[trailingSpaceMidpoint].m_pos--;

        // Now make sure every single trailingPositionedBox following the trailingSpaceMidpoint properly stops and starts
        // ignoring spaces.
        size_t currentMidpoint = trailingSpaceMidpoint + 1;
        for (size_t i = 0; i < m_boxes.size(); ++i) {
            if (currentMidpoint >= lineMidpointState.numMidpoints) {
                // We don't have a midpoint for this box yet.
                ensureLineBoxInsideIgnoredSpaces(lineMidpointState, m_boxes[i]);
            } else {
                ASSERT(lineMidpointState.midpoints[currentMidpoint].object() == m_boxes[i]);
                ASSERT(lineMidpointState.midpoints[currentMidpoint + 1].object() == m_boxes[i]);
            }
            currentMidpoint += 2;
        }
    } else if (!lBreak.object()) {
        ASSERT(m_whitespace->isText());
        ASSERT(collapseFirstSpace == CollapseFirstSpace);
        // Add a new end midpoint that stops right at the very end.
        unsigned length = m_whitespace->textLength();
        unsigned pos = length >= 2 ? length - 2 : UINT_MAX;
        InlineIterator endMid(0, m_whitespace, pos);
        startIgnoringSpaces(lineMidpointState, endMid);
        for (size_t i = 0; i < m_boxes.size(); ++i) {
            ensureLineBoxInsideIgnoredSpaces(lineMidpointState, m_boxes[i]);
        }
    }
}

class BreakingContext {
public:
    BreakingContext(InlineBidiResolver& resolver, LineInfo& inLineInfo, LineWidth& lineWidth, RenderTextInfo& inRenderTextInfo, FloatingObject* inLastFloatFromPreviousLine, bool appliedStartWidth, RenderBlockFlow* block)
        : m_resolver(resolver)
        , m_current(resolver.position())
        , m_lineBreak(resolver.position())
        , m_block(block)
        , m_lastObject(m_current.object())
        , m_nextObject(0)
        , m_currentStyle(0)
        , m_blockStyle(block->style())
        , m_lineInfo(inLineInfo)
        , m_renderTextInfo(inRenderTextInfo)
        , m_lastFloatFromPreviousLine(inLastFloatFromPreviousLine)
        , m_width(lineWidth)
        , m_currWS(NORMAL)
        , m_lastWS(NORMAL)
        , m_preservesNewline(false)
        , m_atStart(true)
        , m_ignoringSpaces(false)
        , m_currentCharacterIsSpace(false)
        , m_currentCharacterShouldCollapseIfPreWap(false)
        , m_appliedStartWidth(appliedStartWidth)
        , m_includeEndWidth(true)
        , m_autoWrap(false)
        , m_autoWrapWasEverTrueOnLine(false)
        , m_floatsFitOnLine(true)
        , m_collapseWhiteSpace(false)
        , m_startingNewParagraph(m_lineInfo.previousLineBrokeCleanly())
        , m_allowImagesToBreak(!block->document().inQuirksMode() || !block->isTableCell() || !m_blockStyle->logicalWidth().isIntrinsicOrAuto())
        , m_atEnd(false)
        , m_lineMidpointState(resolver.midpointState())
    {
        m_lineInfo.setPreviousLineBrokeCleanly(false);
    }

    RenderObject* currentObject() { return m_current.object(); }
    InlineIterator lineBreak() { return m_lineBreak; }
    bool atEnd() { return m_atEnd; }

    void initializeForCurrentObject();

    void increment();

    void handleBR(EClear&);
    void handleOutOfFlowPositioned(Vector<RenderBox*>& positionedObjects);
    void handleFloat();
    void handleEmptyInline();
    void handleReplaced();
    bool handleText(WordMeasurements&, bool& hyphenated);
    void commitAndUpdateLineBreakIfNeeded();
    InlineIterator handleEndOfLine();

    void clearLineBreakIfFitsOnLine()
    {
        if (m_width.fitsOnLine() || m_lastWS == NOWRAP)
            m_lineBreak.clear();
    }

private:
    void skipTrailingWhitespace(InlineIterator&, const LineInfo&);

    InlineBidiResolver& m_resolver;

    InlineIterator m_current;
    InlineIterator m_lineBreak;
    InlineIterator m_startOfIgnoredSpaces;

    RenderBlockFlow* m_block;
    RenderObject* m_lastObject;
    RenderObject* m_nextObject;

    RenderStyle* m_currentStyle;
    RenderStyle* m_blockStyle;

    LineInfo& m_lineInfo;

    RenderTextInfo& m_renderTextInfo;

    FloatingObject* m_lastFloatFromPreviousLine;

    LineWidth m_width;

    EWhiteSpace m_currWS;
    EWhiteSpace m_lastWS;

    bool m_preservesNewline;
    bool m_atStart;
    bool m_ignoringSpaces;
    bool m_currentCharacterIsSpace;
    bool m_currentCharacterShouldCollapseIfPreWap;
    bool m_appliedStartWidth;
    bool m_includeEndWidth;
    bool m_autoWrap;
    bool m_autoWrapWasEverTrueOnLine;
    bool m_floatsFitOnLine;
    bool m_collapseWhiteSpace;
    bool m_startingNewParagraph;
    bool m_allowImagesToBreak;
    bool m_atEnd;

    LineMidpointState& m_lineMidpointState;

    TrailingObjects m_trailingObjects;
};

enum WhitespacePosition { LeadingWhitespace, TrailingWhitespace };

inline bool shouldCollapseWhiteSpace(const RenderStyle* style, const LineInfo& lineInfo, WhitespacePosition whitespacePosition)
{
    // CSS2 16.6.1
    // If a space (U+0020) at the beginning of a line has 'white-space' set to 'normal', 'nowrap', or 'pre-line', it is removed.
    // If a space (U+0020) at the end of a line has 'white-space' set to 'normal', 'nowrap', or 'pre-line', it is also removed.
    // If spaces (U+0020) or tabs (U+0009) at the end of a line have 'white-space' set to 'pre-wrap', UAs may visually collapse them.
    return style->collapseWhiteSpace()
        || (whitespacePosition == TrailingWhitespace && style->whiteSpace() == PRE_WRAP && (!lineInfo.isEmpty() || !lineInfo.previousLineBrokeCleanly()));
}

inline bool requiresLineBoxForContent(RenderInline* flow, const LineInfo& lineInfo)
{
    RenderObject* parent = flow->parent();
    if (flow->document().inNoQuirksMode()
        && (flow->style(lineInfo.isFirstLine())->lineHeight() != parent->style(lineInfo.isFirstLine())->lineHeight()
        || flow->style()->verticalAlign() != parent->style()->verticalAlign()
        || !parent->style()->font().fontMetrics().hasIdenticalAscentDescentAndLineGap(flow->style()->font().fontMetrics())))
        return true;
    return false;
}

inline bool alwaysRequiresLineBox(RenderObject* flow)
{
    // FIXME: Right now, we only allow line boxes for inlines that are truly empty.
    // We need to fix this, though, because at the very least, inlines containing only
    // ignorable whitespace should should also have line boxes.
    return isEmptyInline(flow) && toRenderInline(flow)->hasInlineDirectionBordersPaddingOrMargin();
}

inline bool requiresLineBox(const InlineIterator& it, const LineInfo& lineInfo = LineInfo(), WhitespacePosition whitespacePosition = LeadingWhitespace)
{
    if (it.object()->isFloatingOrOutOfFlowPositioned())
        return false;

    if (it.object()->isRenderInline() && !alwaysRequiresLineBox(it.object()) && !requiresLineBoxForContent(toRenderInline(it.object()), lineInfo))
        return false;

    if (!shouldCollapseWhiteSpace(it.object()->style(), lineInfo, whitespacePosition) || it.object()->isBR())
        return true;

    UChar current = it.current();
    bool notJustWhitespace = current != ' ' && current != '\t' && current != softHyphen && (current != '\n' || it.object()->preservesNewline());
    return notJustWhitespace || isEmptyInline(it.object());
}

inline void setStaticPositions(RenderBlockFlow* block, RenderBox* child)
{
    // FIXME: The math here is actually not really right. It's a best-guess approximation that
    // will work for the common cases
    RenderObject* containerBlock = child->container();
    LayoutUnit blockHeight = block->logicalHeight();
    if (containerBlock->isRenderInline()) {
        // A relative positioned inline encloses us. In this case, we also have to determine our
        // position as though we were an inline. Set |staticInlinePosition| and |staticBlockPosition| on the relative positioned
        // inline so that we can obtain the value later.
        toRenderInline(containerBlock)->layer()->setStaticInlinePosition(block->startAlignedOffsetForLine(blockHeight, false));
        toRenderInline(containerBlock)->layer()->setStaticBlockPosition(blockHeight);
    }
    block->updateStaticInlinePositionForChild(child, blockHeight);
    child->layer()->setStaticBlockPosition(blockHeight);
}

// FIXME: The entire concept of the skipTrailingWhitespace function is flawed, since we really need to be building
// line boxes even for containers that may ultimately collapse away. Otherwise we'll never get positioned
// elements quite right. In other words, we need to build this function's work into the normal line
// object iteration process.
// NB. this function will insert any floating elements that would otherwise
// be skipped but it will not position them.
inline void BreakingContext::skipTrailingWhitespace(InlineIterator& iterator, const LineInfo& lineInfo)
{
    while (!iterator.atEnd() && !requiresLineBox(iterator, lineInfo, TrailingWhitespace)) {
        RenderObject* object = iterator.object();
        if (object->isOutOfFlowPositioned())
            setStaticPositions(m_block, toRenderBox(object));
        else if (object->isFloating())
            m_block->insertFloatingObject(toRenderBox(object));
        iterator.increment();
    }
}

inline void BreakingContext::initializeForCurrentObject()
{
    m_currentStyle = m_current.object()->style();
    m_nextObject = bidiNextSkippingEmptyInlines(m_block, m_current.object());
    if (m_nextObject && m_nextObject->parent() && !m_nextObject->parent()->isDescendantOf(m_current.object()->parent()))
        m_includeEndWidth = true;

    m_currWS = m_current.object()->isReplaced() ? m_current.object()->parent()->style()->whiteSpace() : m_currentStyle->whiteSpace();
    m_lastWS = m_lastObject->isReplaced() ? m_lastObject->parent()->style()->whiteSpace() : m_lastObject->style()->whiteSpace();

    m_autoWrap = RenderStyle::autoWrap(m_currWS);
    m_autoWrapWasEverTrueOnLine = m_autoWrapWasEverTrueOnLine || m_autoWrap;

    m_preservesNewline = m_current.object()->isSVGInlineText() ? false : RenderStyle::preserveNewline(m_currWS);

    m_collapseWhiteSpace = RenderStyle::collapseWhiteSpace(m_currWS);
}

inline void BreakingContext::increment()
{
    // Clear out our character space bool, since inline <pre>s don't collapse whitespace
    // with adjacent inline normal/nowrap spans.
    if (!m_collapseWhiteSpace)
        m_currentCharacterIsSpace = false;

    m_current.moveToStartOf(m_nextObject);
    m_atStart = false;
}

inline void BreakingContext::handleBR(EClear& clear)
{
    if (m_width.fitsOnLine()) {
        RenderObject* br = m_current.object();
        m_lineBreak.moveToStartOf(br);
        m_lineBreak.increment();

        // A <br> always breaks a line, so don't let the line be collapsed
        // away. Also, the space at the end of a line with a <br> does not
        // get collapsed away. It only does this if the previous line broke
        // cleanly. Otherwise the <br> has no effect on whether the line is
        // empty or not.
        if (m_startingNewParagraph)
            m_lineInfo.setEmpty(false, m_block, &m_width);
        m_trailingObjects.clear();
        m_lineInfo.setPreviousLineBrokeCleanly(true);

        // A <br> with clearance always needs a linebox in case the lines below it get dirtied later and
        // need to check for floats to clear - so if we're ignoring spaces, stop ignoring them and add a
        // run for this object.
        if (m_ignoringSpaces && m_currentStyle->clear() != CNONE)
            ensureLineBoxInsideIgnoredSpaces(m_lineMidpointState, br);

        if (!m_lineInfo.isEmpty())
            clear = m_currentStyle->clear();
    }
    m_atEnd = true;
}

inline LayoutUnit borderPaddingMarginStart(RenderInline* child)
{
    return child->marginStart() + child->paddingStart() + child->borderStart();
}

inline LayoutUnit borderPaddingMarginEnd(RenderInline* child)
{
    return child->marginEnd() + child->paddingEnd() + child->borderEnd();
}

inline bool shouldAddBorderPaddingMargin(RenderObject* child, bool &checkSide)
{
    if (!child || (child->isText() && !toRenderText(child)->textLength()))
        return true;
    checkSide = false;
    return checkSide;
}

inline LayoutUnit inlineLogicalWidth(RenderObject* child, bool start = true, bool end = true)
{
    unsigned lineDepth = 1;
    LayoutUnit extraWidth = 0;
    RenderObject* parent = child->parent();
    while (parent->isRenderInline() && lineDepth++ < cMaxLineDepth) {
        RenderInline* parentAsRenderInline = toRenderInline(parent);
        if (!isEmptyInline(parentAsRenderInline)) {
            if (start && shouldAddBorderPaddingMargin(child->previousSibling(), start))
                extraWidth += borderPaddingMarginStart(parentAsRenderInline);
            if (end && shouldAddBorderPaddingMargin(child->nextSibling(), end))
                extraWidth += borderPaddingMarginEnd(parentAsRenderInline);
            if (!start && !end)
                return extraWidth;
        }
        child = parent;
        parent = child->parent();
    }
    return extraWidth;
}

inline void BreakingContext::handleOutOfFlowPositioned(Vector<RenderBox*>& positionedObjects)
{
    // If our original display wasn't an inline type, then we can
    // go ahead and determine our static inline position now.
    RenderBox* box = toRenderBox(m_current.object());
    bool isInlineType = box->style()->isOriginalDisplayInlineType();
    if (!isInlineType) {
        m_block->setStaticInlinePositionForChild(box, m_block->logicalHeight(), m_block->startOffsetForContent(m_block->logicalHeight()));
    } else {
        // If our original display was an INLINE type, then we can go ahead
        // and determine our static y position now.
        box->layer()->setStaticBlockPosition(m_block->logicalHeight());
    }

    // If we're ignoring spaces, we have to stop and include this object and
    // then start ignoring spaces again.
    if (isInlineType || box->container()->isRenderInline()) {
        if (m_ignoringSpaces)
            ensureLineBoxInsideIgnoredSpaces(m_lineMidpointState, box);
        m_trailingObjects.appendBoxIfNeeded(box);
    } else {
        positionedObjects.append(box);
    }
    m_width.addUncommittedWidth(inlineLogicalWidth(box));
    // Reset prior line break context characters.
    m_renderTextInfo.m_lineBreakIterator.resetPriorContext();
}

inline void BreakingContext::handleFloat()
{
    RenderBox* floatBox = toRenderBox(m_current.object());
    FloatingObject* floatingObject = m_block->insertFloatingObject(floatBox);
    // check if it fits in the current line.
    // If it does, position it now, otherwise, position
    // it after moving to next line (in newLine() func)
    // FIXME: Bug 110372: Properly position multiple stacked floats with non-rectangular shape outside.
    if (m_floatsFitOnLine && m_width.fitsOnLine(m_block->logicalWidthForFloat(floatingObject))) {
        m_block->positionNewFloatOnLine(floatingObject, m_lastFloatFromPreviousLine, m_lineInfo, m_width);
        if (m_lineBreak.object() == m_current.object()) {
            ASSERT(!m_lineBreak.m_pos);
            m_lineBreak.increment();
        }
    } else {
        m_floatsFitOnLine = false;
    }
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for floating element.
    m_renderTextInfo.m_lineBreakIterator.updatePriorContext(replacementCharacter);
}

// This is currently just used for list markers and inline flows that have line boxes. Neither should
// have an effect on whitespace at the start of the line.
inline bool shouldSkipWhitespaceAfterStartObject(RenderBlockFlow* block, RenderObject* o, LineMidpointState& lineMidpointState)
{
    RenderObject* next = bidiNextSkippingEmptyInlines(block, o);
    while (next && next->isFloatingOrOutOfFlowPositioned())
        next = bidiNextSkippingEmptyInlines(block, next);

    if (next && !next->isBR() && next->isText() && toRenderText(next)->textLength() > 0) {
        RenderText* nextText = toRenderText(next);
        UChar nextChar = nextText->characterAt(0);
        if (nextText->style()->isCollapsibleWhiteSpace(nextChar)) {
            startIgnoringSpaces(lineMidpointState, InlineIterator(0, o, 0));
            return true;
        }
    }

    return false;
}

inline void BreakingContext::handleEmptyInline()
{
    // This should only end up being called on empty inlines
    ASSERT(isEmptyInline(m_current.object()));

    RenderInline* flowBox = toRenderInline(m_current.object());

    // Now that some inline flows have line boxes, if we are already ignoring spaces, we need
    // to make sure that we stop to include this object and then start ignoring spaces again.
    // If this object is at the start of the line, we need to behave like list markers and
    // start ignoring spaces.
    bool requiresLineBox = alwaysRequiresLineBox(m_current.object());
    if (requiresLineBox || requiresLineBoxForContent(flowBox, m_lineInfo)) {
        // An empty inline that only has line-height, vertical-align or font-metrics will only get a
        // line box to affect the height of the line if the rest of the line is not empty.
        if (requiresLineBox)
            m_lineInfo.setEmpty(false, m_block, &m_width);
        if (m_ignoringSpaces) {
            m_trailingObjects.clear();
            ensureLineBoxInsideIgnoredSpaces(m_lineMidpointState, m_current.object());
        } else if (m_blockStyle->collapseWhiteSpace() && m_resolver.position().object() == m_current.object()
            && shouldSkipWhitespaceAfterStartObject(m_block, m_current.object(), m_lineMidpointState)) {
            // Like with list markers, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterShouldCollapseIfPreWap = m_currentCharacterIsSpace = true;
            m_ignoringSpaces = true;
        }
    }

    m_width.addUncommittedWidth(inlineLogicalWidth(m_current.object()) + borderPaddingMarginStart(flowBox) + borderPaddingMarginEnd(flowBox));
}

inline void BreakingContext::handleReplaced()
{
    RenderBox* replacedBox = toRenderBox(m_current.object());

    if (m_atStart)
        m_width.updateAvailableWidth(replacedBox->logicalHeight());

    // Break on replaced elements if either has normal white-space.
    if ((m_autoWrap || RenderStyle::autoWrap(m_lastWS)) && (!m_current.object()->isImage() || m_allowImagesToBreak)) {
        m_width.commit();
        m_lineBreak.moveToStartOf(m_current.object());
    }

    if (m_ignoringSpaces)
        stopIgnoringSpaces(m_lineMidpointState, InlineIterator(0, m_current.object(), 0));

    m_lineInfo.setEmpty(false, m_block, &m_width);
    m_ignoringSpaces = false;
    m_currentCharacterShouldCollapseIfPreWap = m_currentCharacterIsSpace = false;
    m_trailingObjects.clear();

    // Optimize for a common case. If we can't find whitespace after the list
    // item, then this is all moot.
    LayoutUnit replacedLogicalWidth = m_block->logicalWidthForChild(replacedBox) + m_block->marginStartForChild(replacedBox) + m_block->marginEndForChild(replacedBox) + inlineLogicalWidth(m_current.object());
    if (m_current.object()->isListMarker()) {
        if (m_blockStyle->collapseWhiteSpace() && shouldSkipWhitespaceAfterStartObject(m_block, m_current.object(), m_lineMidpointState)) {
            // Like with inline flows, we start ignoring spaces to make sure that any
            // additional spaces we see will be discarded.
            m_currentCharacterShouldCollapseIfPreWap = m_currentCharacterIsSpace = true;
            m_ignoringSpaces = true;
        }
        if (toRenderListMarker(m_current.object())->isInside())
            m_width.addUncommittedWidth(replacedLogicalWidth);
    } else {
        m_width.addUncommittedWidth(replacedLogicalWidth);
    }
    if (m_current.object()->isRubyRun())
        m_width.applyOverhang(toRenderRubyRun(m_current.object()), m_lastObject, m_nextObject);
    // Update prior line break context characters, using U+FFFD (OBJECT REPLACEMENT CHARACTER) for replaced element.
    m_renderTextInfo.m_lineBreakIterator.updatePriorContext(replacementCharacter);
}

inline bool iteratorIsBeyondEndOfRenderCombineText(const InlineIterator& iter, RenderCombineText* renderer)
{
    return iter.object() == renderer && iter.m_pos >= renderer->textLength();
}

inline void nextCharacter(UChar& currentCharacter, UChar& lastCharacter, UChar& secondToLastCharacter)
{
    secondToLastCharacter = lastCharacter;
    lastCharacter = currentCharacter;
}

inline float firstPositiveWidth(const WordMeasurements& wordMeasurements)
{
    for (size_t i = 0; i < wordMeasurements.size(); ++i) {
        if (wordMeasurements[i].width > 0)
            return wordMeasurements[i].width;
    }
    return 0;
}

inline void updateSegmentsForShapes(RenderBlockFlow* block, const FloatingObject* lastFloatFromPreviousLine, const WordMeasurements& wordMeasurements, LineWidth& width, bool isFirstLine)
{
    ASSERT(lastFloatFromPreviousLine);

    ShapeInsideInfo* shapeInsideInfo = block->layoutShapeInsideInfo();
    if (!lastFloatFromPreviousLine->isPlaced() || !shapeInsideInfo)
        return;

    bool isHorizontalWritingMode = block->isHorizontalWritingMode();
    LayoutUnit logicalOffsetFromShapeContainer = block->logicalOffsetFromShapeAncestorContainer(shapeInsideInfo->owner()).height();

    LayoutUnit lineLogicalTop = block->logicalHeight() + logicalOffsetFromShapeContainer;
    LayoutUnit lineLogicalHeight = block->lineHeight(isFirstLine, isHorizontalWritingMode ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    LayoutUnit lineLogicalBottom = lineLogicalTop + lineLogicalHeight;

    LayoutUnit floatLogicalTop = block->logicalTopForFloat(lastFloatFromPreviousLine);
    LayoutUnit floatLogicalBottom = block->logicalBottomForFloat(lastFloatFromPreviousLine);

    bool lineOverlapsWithFloat = (floatLogicalTop < lineLogicalBottom) && (lineLogicalTop < floatLogicalBottom);
    if (!lineOverlapsWithFloat)
        return;

    float minSegmentWidth = firstPositiveWidth(wordMeasurements);

    LayoutUnit floatLogicalWidth = block->logicalWidthForFloat(lastFloatFromPreviousLine);
    LayoutUnit availableLogicalWidth = block->logicalWidth() - block->logicalRightForFloat(lastFloatFromPreviousLine);
    if (availableLogicalWidth < minSegmentWidth)
        block->setLogicalHeight(floatLogicalBottom);

    if (block->logicalHeight() < floatLogicalTop) {
        shapeInsideInfo->adjustLogicalLineTop(minSegmentWidth + floatLogicalWidth);
        block->setLogicalHeight(shapeInsideInfo->logicalLineTop() - logicalOffsetFromShapeContainer);
    }

    lineLogicalTop = block->logicalHeight() + logicalOffsetFromShapeContainer;

    shapeInsideInfo->updateSegmentsForLine(lineLogicalTop, lineLogicalHeight);
    width.updateCurrentShapeSegment();
    width.updateAvailableWidth();
}

inline float measureHyphenWidth(RenderText* renderer, const Font& font)
{
    RenderStyle* style = renderer->style();
    return font.width(RenderBlockFlow::constructTextRun(renderer, font, style->hyphenString().string(), style));
}

ALWAYS_INLINE float textWidth(RenderText* text, unsigned from, unsigned len, const Font& font, float xPos, bool isFixedPitch, bool collapseWhiteSpace, HashSet<const SimpleFontData*>* fallbackFonts = 0, TextLayout* layout = 0)
{
    GlyphOverflow glyphOverflow;
    if (isFixedPitch || (!from && len == text->textLength()) || text->style()->hasTextCombine())
        return text->width(from, len, font, xPos, fallbackFonts, &glyphOverflow);

    if (layout)
        return Font::width(*layout, from, len, fallbackFonts);

    TextRun run = RenderBlockFlow::constructTextRun(text, font, text, from, len, text->style());
    run.setCharactersLength(text->textLength() - from);
    ASSERT(run.charactersLength() >= run.length());

    run.setCharacterScanForCodePath(!text->canUseSimpleFontCodePath());
    run.setTabSize(!collapseWhiteSpace, text->style()->tabSize());
    run.setXPos(xPos);
    return font.width(run, fallbackFonts, &glyphOverflow);
}

inline bool BreakingContext::handleText(WordMeasurements& wordMeasurements, bool& hyphenated)
{
    if (!m_current.m_pos)
        m_appliedStartWidth = false;

    RenderText* renderText = toRenderText(m_current.object());

    bool isSVGText = renderText->isSVGInlineText();

    if (renderText->style()->hasTextCombine() && m_current.object()->isCombineText() && !toRenderCombineText(m_current.object())->isCombined()) {
        RenderCombineText* combineRenderer = toRenderCombineText(m_current.object());
        combineRenderer->combineText();
        // The length of the renderer's text may have changed. Increment stale iterator positions
        if (iteratorIsBeyondEndOfRenderCombineText(m_lineBreak, combineRenderer)) {
            ASSERT(iteratorIsBeyondEndOfRenderCombineText(m_resolver.position(), combineRenderer));
            m_lineBreak.increment();
            m_resolver.position().increment(&m_resolver);
        }
    }

    RenderStyle* style = renderText->style(m_lineInfo.isFirstLine());
    const Font& font = style->font();
    bool isFixedPitch = font.isFixedPitch();

    unsigned lastSpace = m_current.m_pos;
    float wordSpacing = m_currentStyle->wordSpacing();
    float lastSpaceWordSpacing = 0;
    float wordSpacingForWordMeasurement = 0;

    float wrapW = m_width.uncommittedWidth() + inlineLogicalWidth(m_current.object(), !m_appliedStartWidth, true);
    float charWidth = 0;
    // Auto-wrapping text should wrap in the middle of a word only if it could not wrap before the word,
    // which is only possible if the word is the first thing on the line, that is, if |w| is zero.
    bool breakWords = m_currentStyle->breakWords() && ((m_autoWrap && !m_width.committedWidth()) || m_currWS == PRE);
    bool midWordBreak = false;
    bool breakAll = m_currentStyle->wordBreak() == BreakAllWordBreak && m_autoWrap;
    float hyphenWidth = 0;

    if (isSVGText) {
        breakWords = false;
        breakAll = false;
    }

    if (renderText->isWordBreak()) {
        m_width.commit();
        m_lineBreak.moveToStartOf(m_current.object());
        ASSERT(m_current.m_pos == renderText->textLength());
    }

    if (m_renderTextInfo.m_text != renderText) {
        m_renderTextInfo.m_text = renderText;
        m_renderTextInfo.m_font = &font;
        m_renderTextInfo.createLayout(renderText, m_width.currentWidth(), m_collapseWhiteSpace);
        m_renderTextInfo.m_lineBreakIterator.resetStringAndReleaseIterator(renderText->text(), style->locale());
    } else if (m_renderTextInfo.m_layout && m_renderTextInfo.m_font != &font) {
        m_renderTextInfo.m_font = &font;
        m_renderTextInfo.createLayout(renderText, m_width.currentWidth(), m_collapseWhiteSpace);
    }

    TextLayout* textLayout = m_renderTextInfo.m_layout.get();

    // Non-zero only when kerning is enabled and TextLayout isn't used, in which case we measure
    // words with their trailing space, then subtract its width.
    float wordTrailingSpaceWidth = (font.typesettingFeatures() & Kerning) && !textLayout ? font.width(RenderBlockFlow::constructTextRun(renderText, font, &space, 1, style)) + wordSpacing : 0;

    UChar lastCharacter = m_renderTextInfo.m_lineBreakIterator.lastCharacter();
    UChar secondToLastCharacter = m_renderTextInfo.m_lineBreakIterator.secondToLastCharacter();
    for (; m_current.m_pos < renderText->textLength(); m_current.fastIncrementInTextNode()) {
        bool previousCharacterIsSpace = m_currentCharacterIsSpace;
        bool previousCharacterShouldCollapseIfPreWap = m_currentCharacterShouldCollapseIfPreWap;
        UChar c = m_current.current();
        m_currentCharacterShouldCollapseIfPreWap = m_currentCharacterIsSpace = c == ' ' || c == '\t' || (!m_preservesNewline && (c == '\n'));

        if (!m_collapseWhiteSpace || !m_currentCharacterIsSpace)
            m_lineInfo.setEmpty(false, m_block, &m_width);

        if (c == softHyphen && m_autoWrap && !hyphenWidth) {
            hyphenWidth = measureHyphenWidth(renderText, font);
            m_width.addUncommittedWidth(hyphenWidth);
        }

        bool applyWordSpacing = false;

        if ((breakAll || breakWords) && !midWordBreak) {
            wrapW += charWidth;
            bool midWordBreakIsBeforeSurrogatePair = U16_IS_LEAD(c) && m_current.m_pos + 1 < renderText->textLength() && U16_IS_TRAIL((*renderText)[m_current.m_pos + 1]);
            charWidth = textWidth(renderText, m_current.m_pos, midWordBreakIsBeforeSurrogatePair ? 2 : 1, font, m_width.committedWidth() + wrapW, isFixedPitch, m_collapseWhiteSpace, 0, textLayout);
            midWordBreak = m_width.committedWidth() + wrapW + charWidth > m_width.availableWidth();
        }

        bool betweenWords = c == '\n' || (m_currWS != PRE && !m_atStart && isBreakable(m_renderTextInfo.m_lineBreakIterator, m_current.m_pos, m_current.m_nextBreakablePosition));

        if (betweenWords || midWordBreak) {
            bool stoppedIgnoringSpaces = false;
            if (m_ignoringSpaces) {
                lastSpaceWordSpacing = 0;
                if (!m_currentCharacterIsSpace) {
                    // Stop ignoring spaces and begin at this
                    // new point.
                    m_ignoringSpaces = false;
                    wordSpacingForWordMeasurement = 0;
                    lastSpace = m_current.m_pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
                    stopIgnoringSpaces(m_lineMidpointState, InlineIterator(0, m_current.object(), m_current.m_pos));
                    stoppedIgnoringSpaces = true;
                } else {
                    // Just keep ignoring these spaces.
                    nextCharacter(c, lastCharacter, secondToLastCharacter);
                    continue;
                }
            }

            wordMeasurements.grow(wordMeasurements.size() + 1);
            WordMeasurement& wordMeasurement = wordMeasurements.last();

            wordMeasurement.renderer = renderText;
            wordMeasurement.endOffset = m_current.m_pos;
            wordMeasurement.startOffset = lastSpace;

            float additionalTmpW;
            if (wordTrailingSpaceWidth && c == ' ')
                additionalTmpW = textWidth(renderText, lastSpace, m_current.m_pos + 1 - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, textLayout) - wordTrailingSpaceWidth;
            else
                additionalTmpW = textWidth(renderText, lastSpace, m_current.m_pos - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, textLayout);

            wordMeasurement.width = additionalTmpW + wordSpacingForWordMeasurement;
            additionalTmpW += lastSpaceWordSpacing;
            m_width.addUncommittedWidth(additionalTmpW);
            if (!m_appliedStartWidth) {
                m_width.addUncommittedWidth(inlineLogicalWidth(m_current.object(), true, false));
                m_appliedStartWidth = true;
            }

            if (m_lastFloatFromPreviousLine)
                updateSegmentsForShapes(m_block, m_lastFloatFromPreviousLine, wordMeasurements, m_width, m_lineInfo.isFirstLine());

            applyWordSpacing = wordSpacing && m_currentCharacterIsSpace;

            if (!m_width.committedWidth() && m_autoWrap && !m_width.fitsOnLine())
                m_width.fitBelowFloats();

            if (m_autoWrap || breakWords) {
                // If we break only after white-space, consider the current character
                // as candidate width for this line.
                bool lineWasTooWide = false;
                if (m_width.fitsOnLine() && m_currentCharacterIsSpace && m_currentStyle->breakOnlyAfterWhiteSpace() && !midWordBreak) {
                    float charWidth = textWidth(renderText, m_current.m_pos, 1, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, textLayout) + (applyWordSpacing ? wordSpacing : 0);
                    // Check if line is too big even without the extra space
                    // at the end of the line. If it is not, do nothing.
                    // If the line needs the extra whitespace to be too long,
                    // then move the line break to the space and skip all
                    // additional whitespace.
                    if (!m_width.fitsOnLine(charWidth)) {
                        lineWasTooWide = true;
                        m_lineBreak.moveTo(m_current.object(), m_current.m_pos, m_current.m_nextBreakablePosition);
                        skipTrailingWhitespace(m_lineBreak, m_lineInfo);
                    }
                }
                if (lineWasTooWide || !m_width.fitsOnLine()) {
                    if (m_lineBreak.atTextParagraphSeparator()) {
                        if (!stoppedIgnoringSpaces && m_current.m_pos > 0)
                            ensureCharacterGetsLineBox(m_lineMidpointState, m_current);
                        m_lineBreak.increment();
                        m_lineInfo.setPreviousLineBrokeCleanly(true);
                        wordMeasurement.endOffset = m_lineBreak.m_pos;
                    }
                    if (m_lineBreak.object() && m_lineBreak.m_pos && m_lineBreak.object()->isText() && toRenderText(m_lineBreak.object())->textLength() && toRenderText(m_lineBreak.object())->characterAt(m_lineBreak.m_pos - 1) == softHyphen)
                        hyphenated = true;
                    if (m_lineBreak.m_pos && m_lineBreak.m_pos != (unsigned)wordMeasurement.endOffset && !wordMeasurement.width) {
                        if (charWidth) {
                            wordMeasurement.endOffset = m_lineBreak.m_pos;
                            wordMeasurement.width = charWidth;
                        }
                    }
                    // Didn't fit. Jump to the end unless there's still an opportunity to collapse whitespace.
                    if (m_ignoringSpaces || !m_collapseWhiteSpace || !m_currentCharacterIsSpace || !previousCharacterIsSpace) {
                        m_atEnd = true;
                        return false;
                    }
                } else {
                    if (!betweenWords || (midWordBreak && !m_autoWrap))
                        m_width.addUncommittedWidth(-additionalTmpW);
                    if (hyphenWidth) {
                        // Subtract the width of the soft hyphen out since we fit on a line.
                        m_width.addUncommittedWidth(-hyphenWidth);
                        hyphenWidth = 0;
                    }
                }
            }

            if (c == '\n' && m_preservesNewline) {
                if (!stoppedIgnoringSpaces && m_current.m_pos > 0)
                    ensureCharacterGetsLineBox(m_lineMidpointState, m_current);
                m_lineBreak.moveTo(m_current.object(), m_current.m_pos, m_current.m_nextBreakablePosition);
                m_lineBreak.increment();
                m_lineInfo.setPreviousLineBrokeCleanly(true);
                return true;
            }

            if (m_autoWrap && betweenWords) {
                m_width.commit();
                wrapW = 0;
                m_lineBreak.moveTo(m_current.object(), m_current.m_pos, m_current.m_nextBreakablePosition);
                // Auto-wrapping text should not wrap in the middle of a word once it has had an
                // opportunity to break after a word.
                breakWords = false;
            }

            if (midWordBreak && !U16_IS_TRAIL(c) && !(category(c) & (Mark_NonSpacing | Mark_Enclosing | Mark_SpacingCombining))) {
                // Remember this as a breakable position in case
                // adding the end width forces a break.
                m_lineBreak.moveTo(m_current.object(), m_current.m_pos, m_current.m_nextBreakablePosition);
                midWordBreak &= (breakWords || breakAll);
            }

            if (betweenWords) {
                lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
                wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurement.width) ? wordSpacing : 0;
                lastSpace = m_current.m_pos;
            }

            if (!m_ignoringSpaces && m_currentStyle->collapseWhiteSpace()) {
                // If we encounter a newline, or if we encounter a
                // second space, we need to go ahead and break up this
                // run and enter a mode where we start collapsing spaces.
                if (m_currentCharacterIsSpace && previousCharacterIsSpace) {
                    m_ignoringSpaces = true;

                    // We just entered a mode where we are ignoring
                    // spaces. Create a midpoint to terminate the run
                    // before the second space.
                    startIgnoringSpaces(m_lineMidpointState, m_startOfIgnoredSpaces);
                    m_trailingObjects.updateMidpointsForTrailingBoxes(m_lineMidpointState, InlineIterator(), TrailingObjects::DoNotCollapseFirstSpace);
                }
            }
        } else if (m_ignoringSpaces) {
            // Stop ignoring spaces and begin at this
            // new point.
            m_ignoringSpaces = false;
            lastSpaceWordSpacing = applyWordSpacing ? wordSpacing : 0;
            wordSpacingForWordMeasurement = (applyWordSpacing && wordMeasurements.last().width) ? wordSpacing : 0;
            lastSpace = m_current.m_pos; // e.g., "Foo    goo", don't add in any of the ignored spaces.
            stopIgnoringSpaces(m_lineMidpointState, InlineIterator(0, m_current.object(), m_current.m_pos));
        }

        if (isSVGText && m_current.m_pos > 0) {
            // Force creation of new InlineBoxes for each absolute positioned character (those that start new text chunks).
            if (toRenderSVGInlineText(renderText)->characterStartsNewTextChunk(m_current.m_pos))
                ensureCharacterGetsLineBox(m_lineMidpointState, m_current);
        }

        if (m_currentCharacterIsSpace && !previousCharacterIsSpace) {
            m_startOfIgnoredSpaces.setObject(m_current.object());
            m_startOfIgnoredSpaces.m_pos = m_current.m_pos;
        }

        if (!m_currentCharacterIsSpace && previousCharacterShouldCollapseIfPreWap) {
            if (m_autoWrap && m_currentStyle->breakOnlyAfterWhiteSpace())
                m_lineBreak.moveTo(m_current.object(), m_current.m_pos, m_current.m_nextBreakablePosition);
        }

        if (m_collapseWhiteSpace && m_currentCharacterIsSpace && !m_ignoringSpaces)
            m_trailingObjects.setTrailingWhitespace(toRenderText(m_current.object()));
        else if (!m_currentStyle->collapseWhiteSpace() || !m_currentCharacterIsSpace)
            m_trailingObjects.clear();

        m_atStart = false;
        nextCharacter(c, lastCharacter, secondToLastCharacter);
    }

    m_renderTextInfo.m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);

    wordMeasurements.grow(wordMeasurements.size() + 1);
    WordMeasurement& wordMeasurement = wordMeasurements.last();
    wordMeasurement.renderer = renderText;

    // IMPORTANT: current.m_pos is > length here!
    float additionalTmpW = m_ignoringSpaces ? 0 : textWidth(renderText, lastSpace, m_current.m_pos - lastSpace, font, m_width.currentWidth(), isFixedPitch, m_collapseWhiteSpace, &wordMeasurement.fallbackFonts, textLayout);
    wordMeasurement.startOffset = lastSpace;
    wordMeasurement.endOffset = m_current.m_pos;
    wordMeasurement.width = m_ignoringSpaces ? 0 : additionalTmpW + wordSpacingForWordMeasurement;
    additionalTmpW += lastSpaceWordSpacing;
    m_width.addUncommittedWidth(additionalTmpW + inlineLogicalWidth(m_current.object(), !m_appliedStartWidth, m_includeEndWidth));
    m_includeEndWidth = false;

    if (!m_width.fitsOnLine()) {
        if (!hyphenated && m_lineBreak.previousInSameNode() == softHyphen) {
            hyphenated = true;
            m_atEnd = true;
        }
    }
    return false;
}

inline void BreakingContext::commitAndUpdateLineBreakIfNeeded()
{
    bool checkForBreak = m_autoWrap;
    if (m_width.committedWidth() && !m_width.fitsOnLine() && m_lineBreak.object() && m_currWS == NOWRAP) {
        checkForBreak = true;
    } else if (m_nextObject && m_current.object()->isText() && m_nextObject->isText() && !m_nextObject->isBR() && (m_autoWrap || m_nextObject->style()->autoWrap())) {
        if (m_autoWrap && m_currentCharacterIsSpace) {
            checkForBreak = true;
        } else {
            RenderText* nextText = toRenderText(m_nextObject);
            if (nextText->textLength()) {
                UChar c = nextText->characterAt(0);
                // If the next item on the line is text, and if we did not end with
                // a space, then the next text run continues our word (and so it needs to
                // keep adding to the uncommitted width. Just update and continue.
                checkForBreak = !m_currentCharacterIsSpace && (c == ' ' || c == '\t' || (c == '\n' && !m_nextObject->preservesNewline()));
            } else if (nextText->isWordBreak()) {
                checkForBreak = true;
            }

            if (!m_width.fitsOnLine() && !m_width.committedWidth())
                m_width.fitBelowFloats();

            bool canPlaceOnLine = m_width.fitsOnLine() || !m_autoWrapWasEverTrueOnLine;
            if (canPlaceOnLine && checkForBreak) {
                m_width.commit();
                m_lineBreak.moveToStartOf(m_nextObject);
            }
        }
    }

    if (checkForBreak && !m_width.fitsOnLine()) {
        // if we have floats, try to get below them.
        if (m_currentCharacterIsSpace && !m_ignoringSpaces && m_currentStyle->collapseWhiteSpace())
            m_trailingObjects.clear();

        if (m_width.committedWidth()) {
            m_atEnd = true;
            return;
        }

        m_width.fitBelowFloats();

        // |width| may have been adjusted because we got shoved down past a float (thus
        // giving us more room), so we need to retest, and only jump to
        // the end label if we still don't fit on the line. -dwh
        if (!m_width.fitsOnLine()) {
            m_atEnd = true;
            return;
        }
    } else if (m_blockStyle->autoWrap() && !m_width.fitsOnLine() && !m_width.committedWidth()) {
        // If the container autowraps but the current child does not then we still need to ensure that it
        // wraps and moves below any floats.
        m_width.fitBelowFloats();
    }

    if (!m_current.object()->isFloatingOrOutOfFlowPositioned()) {
        m_lastObject = m_current.object();
        if (m_lastObject->isReplaced() && m_autoWrap && (!m_lastObject->isImage() || m_allowImagesToBreak) && (!m_lastObject->isListMarker() || toRenderListMarker(m_lastObject)->isInside())) {
            m_width.commit();
            m_lineBreak.moveToStartOf(m_nextObject);
        }
    }
}

inline void checkMidpoints(LineMidpointState& lineMidpointState, InlineIterator& lBreak)
{
    // Check to see if our last midpoint is a start point beyond the line break. If so,
    // shave it off the list, and shave off a trailing space if the previous end point doesn't
    // preserve whitespace.
    if (lBreak.object() && lineMidpointState.numMidpoints && !(lineMidpointState.numMidpoints % 2)) {
        InlineIterator* midpoints = lineMidpointState.midpoints.data();
        InlineIterator& endpoint = midpoints[lineMidpointState.numMidpoints - 2];
        const InlineIterator& startpoint = midpoints[lineMidpointState.numMidpoints - 1];
        InlineIterator currpoint = endpoint;
        while (!currpoint.atEnd() && currpoint != startpoint && currpoint != lBreak)
            currpoint.increment();
        if (currpoint == lBreak) {
            // We hit the line break before the start point. Shave off the start point.
            lineMidpointState.numMidpoints--;
            if (endpoint.object()->style()->collapseWhiteSpace() && endpoint.object()->isText())
                endpoint.m_pos--;
        }
    }
}

InlineIterator BreakingContext::handleEndOfLine()
{
    ShapeInsideInfo* shapeInfo = m_block->layoutShapeInsideInfo();
    bool segmentAllowsOverflow = !shapeInfo || !shapeInfo->hasSegments();

    if (m_lineBreak == m_resolver.position() && (!m_lineBreak.object() || !m_lineBreak.object()->isBR()) && segmentAllowsOverflow) {
        // we just add as much as possible
        if (m_blockStyle->whiteSpace() == PRE && !m_current.m_pos) {
            m_lineBreak.moveTo(m_lastObject, m_lastObject->isText() ? m_lastObject->length() : 0);
        } else if (m_lineBreak.object()) {
            // Don't ever break in the middle of a word if we can help it.
            // There's no room at all. We just have to be on this line,
            // even though we'll spill out.
            m_lineBreak.moveTo(m_current.object(), m_current.m_pos);
        }
    }

    // FIXME Bug 100049: We do not need to consume input in a multi-segment line
    // unless no segment will.
    // make sure we consume at least one char/object.
    if (m_lineBreak == m_resolver.position() && segmentAllowsOverflow)
        m_lineBreak.increment();

    // Sanity check our midpoints.
    checkMidpoints(m_lineMidpointState, m_lineBreak);

    m_trailingObjects.updateMidpointsForTrailingBoxes(m_lineMidpointState, m_lineBreak, TrailingObjects::CollapseFirstSpace);

    // We might have made lineBreak an iterator that points past the end
    // of the object. Do this adjustment to make it point to the start
    // of the next object instead to avoid confusing the rest of the
    // code.
    if (m_lineBreak.m_pos > 0) {
        // This loop enforces the invariant that line breaks should never point
        // at an empty inline. See http://crbug.com/305904.
        do {
            m_lineBreak.m_pos--;
            m_lineBreak.increment();
        } while (!m_lineBreak.atEnd() && isEmptyInline(m_lineBreak.object()));
    }

    return m_lineBreak;
}

}

#endif // BreakingContextInlineHeaders_h
