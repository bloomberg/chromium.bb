// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutAnalyzer.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"
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
    m_counters[Depth] = 0;
    m_counters[Total] = 0;
}

void LayoutAnalyzer::push(const LayoutObject& o)
{
    increment(Total);

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
    case Depth: return "Depth";
    case Total: return "Total";
    }
    ASSERT_NOT_REACHED();
    return "";
}

} // namespace blink
