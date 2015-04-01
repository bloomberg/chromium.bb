// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutAnalyzer.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "platform/TracedValue.h"

namespace blink {

LayoutAnalyzer::Scope::Scope(const LayoutObject& o)
    : m_layoutObject(o)
    , m_analyzer(o.frameView()->layoutAnalyzer())
{
    if (UNLIKELY(m_analyzer != nullptr))
        m_analyzer->push(o);
}

LayoutAnalyzer::Scope::~Scope()
{
    if (UNLIKELY(m_analyzer != nullptr))
        m_analyzer->pop(m_layoutObject);
}

void LayoutAnalyzer::reset()
{
    m_counters.resize(NumCounters);
    m_counters[LayoutBlockRectangleChanged] = 0;
    m_counters[LayoutBlockRectangleDidNotChange] = 0;
    m_counters[LayoutObjectsThatSpecifyColumns] = 0;
    m_counters[LayoutAnalyzerStackMaximumDepth] = 0;
    m_counters[LayoutObjectsThatAreFloating] = 0;
    m_counters[LayoutObjectsThatHaveALayer] = 0;
    m_counters[LayoutInlineObjectsThatAlwaysCreateLineBoxes] = 0;
    m_counters[LayoutObjectsThatHadNeverHadLayout] = 0;
    m_counters[LayoutObjectsThatAreOutOfFlowPositioned] = 0;
    m_counters[LayoutObjectsThatNeedPositionedMovementLayout] = 0;
    m_counters[PerformLayoutRootLayoutObjects] = 0;
    m_counters[LayoutObjectsThatNeedLayoutForThemselves] = 0;
    m_counters[LayoutObjectsThatNeedSimplifiedLayout] = 0;
    m_counters[LayoutObjectsThatAreTableCells] = 0;
    m_counters[LayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath] = 0;
    m_counters[CharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath] = 0;
    m_counters[LayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath] = 0;
    m_counters[CharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath] = 0;
    m_counters[TotalLayoutObjectsThatWereLaidOut] = 0;
}

void LayoutAnalyzer::push(const LayoutObject& o)
{
    increment(TotalLayoutObjectsThatWereLaidOut);
    if (!o.everHadLayout())
        increment(LayoutObjectsThatHadNeverHadLayout);
    if (o.selfNeedsLayout())
        increment(LayoutObjectsThatNeedLayoutForThemselves);
    if (o.needsPositionedMovementLayout())
        increment(LayoutObjectsThatNeedPositionedMovementLayout);
    if (o.isOutOfFlowPositioned())
        increment(LayoutObjectsThatAreOutOfFlowPositioned);
    if (o.isTableCell())
        increment(LayoutObjectsThatAreTableCells);
    if (o.isFloating())
        increment(LayoutObjectsThatAreFloating);
    if (o.style()->specifiesColumns())
        increment(LayoutObjectsThatSpecifyColumns);
    if (o.hasLayer())
        increment(LayoutObjectsThatHaveALayer);
    if (o.isLayoutInline() && o.alwaysCreateLineBoxesForLayoutInline())
        increment(LayoutInlineObjectsThatAlwaysCreateLineBoxes);
    if (o.isText()) {
        const LayoutText& t = *toLayoutText(&o);
        if (t.canUseSimpleFontCodePath()) {
            increment(LayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath);
            increment(CharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath, t.textLength());
        } else {
            increment(LayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath);
            increment(CharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath, t.textLength());
        }
    }

    // This might be a root in a subtree layout, in which case the LayoutObject
    // has a parent but the stack is empty. If a LayoutObject subclass forgets
    // to call push() and is a root in a subtree layout, then this
    // assert would only fail if that LayoutObject instance has any children
    // that need layout and do call push().
    // LayoutBlock::layoutPositionedObjects() hoists positioned descendants.
    // LayoutBlockFlow::layoutInlineChildren() walks through inlines.
    // LayoutTableSection::layoutRows() walks through rows.
    if (!o.isPositioned()
        && !o.isTableCell()
        && !o.isSVGResourceContainer()
        && (m_stack.size() != 0)
        && !(o.parent()->childrenInline()
            && (o.isReplaced() || o.isFloating() || o.isOutOfFlowPositioned()))) {
        ASSERT(o.parent() == m_stack.peek());
    }
    m_stack.push(&o);

    // This refers to LayoutAnalyzer depth, not layout tree depth or DOM tree
    // depth. LayoutAnalyzer depth is generally closer to C++ stack recursion
    // depth. See above exceptions for when LayoutAnalyzer depth != layout tree
    // depth.
    if (m_stack.size() > m_counters[LayoutAnalyzerStackMaximumDepth])
        m_counters[LayoutAnalyzerStackMaximumDepth] = m_stack.size();

}

void LayoutAnalyzer::pop(const LayoutObject& o)
{
    ASSERT(m_stack.peek() == &o);
    m_stack.pop();
}

PassRefPtr<TracedValue> LayoutAnalyzer::toTracedValue()
{
    RefPtr<TracedValue> tracedValue(TracedValue::create());
    for (size_t i = 0; i < NumCounters; ++i) {
        if (m_counters[i] > 0)
            tracedValue->setInteger(nameForCounter(static_cast<Counter>(i)), m_counters[i]);
    }
    return tracedValue.release();
}

const char* LayoutAnalyzer::nameForCounter(Counter counter) const
{
    switch (counter) {
    case LayoutBlockRectangleChanged: return "LayoutBlockRectangleChanged";
    case LayoutBlockRectangleDidNotChange: return "LayoutBlockRectangleDidNotChange";
    case LayoutObjectsThatSpecifyColumns: return "LayoutObjectsThatSpecifyColumns";
    case LayoutAnalyzerStackMaximumDepth: return "LayoutAnalyzerStackMaximumDepth";
    case LayoutObjectsThatAreFloating: return "LayoutObjectsThatAreFloating";
    case LayoutObjectsThatHaveALayer: return "LayoutObjectsThatHaveALayer";
    case LayoutInlineObjectsThatAlwaysCreateLineBoxes: return "LayoutInlineObjectsThatAlwaysCreateLineBoxes";
    case LayoutObjectsThatHadNeverHadLayout: return "LayoutObjectsThatHadNeverHadLayout";
    case LayoutObjectsThatAreOutOfFlowPositioned: return "LayoutObjectsThatAreOutOfFlowPositioned";
    case LayoutObjectsThatNeedPositionedMovementLayout: return "LayoutObjectsThatNeedPositionedMovementLayout";
    case PerformLayoutRootLayoutObjects: return "PerformLayoutRootLayoutObjects";
    case LayoutObjectsThatNeedLayoutForThemselves: return "LayoutObjectsThatNeedLayoutForThemselves";
    case LayoutObjectsThatNeedSimplifiedLayout: return "LayoutObjectsThatNeedSimplifiedLayout";
    case LayoutObjectsThatAreTableCells: return "LayoutObjectsThatAreTableCells";
    case LayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath: return "LayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath";
    case CharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath: return "CharactersInLayoutObjectsThatAreTextAndCanNotUseTheSimpleFontCodePath";
    case LayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath: return "LayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath";
    case CharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath: return "CharactersInLayoutObjectsThatAreTextAndCanUseTheSimpleFontCodePath";
    case TotalLayoutObjectsThatWereLaidOut: return "TotalLayoutObjectsThatWereLaidOut";
    }
    ASSERT_NOT_REACHED();
    return "";
}

} // namespace blink
