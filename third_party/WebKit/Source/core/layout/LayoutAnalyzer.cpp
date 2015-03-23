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
    m_counters[BlockRectChanged] = 0;
    m_counters[BlockRectUnchanged] = 0;
    m_counters[Columns] = 0;
    m_counters[Depth] = 0;
    m_counters[Float] = 0;
    m_counters[Layer] = 0;
    m_counters[LineBoxes] = 0;
    m_counters[New] = 0;
    m_counters[OutOfFlow] = 0;
    m_counters[PositionedMovement] = 0;
    m_counters[Roots] = 0;
    m_counters[SelfNeeds] = 0;
    m_counters[TableCell] = 0;
    m_counters[TextComplex] = 0;
    m_counters[TextComplexChar] = 0;
    m_counters[TextSimple] = 0;
    m_counters[TextSimpleChar] = 0;
    m_counters[Total] = 0;
}

void LayoutAnalyzer::push(const LayoutObject& o)
{
    increment(Total);
    if (!o.everHadLayout())
        increment(New);
    if (o.selfNeedsLayout())
        increment(SelfNeeds);
    if (o.needsPositionedMovementLayout())
        increment(PositionedMovement);
    if (o.isOutOfFlowPositioned())
        increment(OutOfFlow);
    if (o.isTableCell())
        increment(TableCell);
    if (o.isFloating())
        increment(Float);
    if (o.style()->specifiesColumns())
        increment(Columns);
    if (o.hasLayer())
        increment(Layer);
    if (o.isLayoutInline() && o.alwaysCreateLineBoxesForLayoutInline())
        increment(LineBoxes);
    if (o.isText()) {
        const LayoutText& t = *toLayoutText(&o);
        if (t.canUseSimpleFontCodePath()) {
            increment(TextSimple);
            increment(TextSimpleChar, t.textLength());
        } else {
            increment(TextComplex);
            increment(TextComplexChar, t.textLength());
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
    if (m_stack.size() > m_counters[Depth])
        m_counters[Depth] = m_stack.size();

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
    case BlockRectChanged: return "BlockRectChanged";
    case BlockRectUnchanged: return "BlockRectUnchanged";
    case Columns: return "Columns";
    case Depth: return "Depth";
    case Float: return "Float";
    case Layer: return "Layer";
    case LineBoxes: return "LineBoxes";
    case New: return "New";
    case OutOfFlow: return "OutOfFlow";
    case PositionedMovement: return "PositionedMovement";
    case Roots: return "Roots";
    case SelfNeeds: return "SelfNeeds";
    case TableCell: return "TableCell";
    case TextComplex: return "TextComplex";
    case TextComplexChar: return "TextComplexChar";
    case TextSimple: return "TextSimple";
    case TextSimpleChar: return "TextSimpleChar";
    case Total: return "Total";
    }
    ASSERT_NOT_REACHED();
    return "";
}

} // namespace blink
