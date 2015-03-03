// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutMultiColumnSpannerPlaceholder_h
#define LayoutMultiColumnSpannerPlaceholder_h

#include "core/layout/LayoutMultiColumnFlowThread.h"

namespace blink {

// Placeholder renderer for column-span:all elements. The column-span:all renderer itself is a
// descendant of the flow thread, but due to its out-of-flow nature, we need something on the
// outside to take care of its positioning and sizing. LayoutMultiColumnSpannerPlaceholder objects
// are siblings of LayoutMultiColumnSet objects, i.e. direct children of the multicol container.
class LayoutMultiColumnSpannerPlaceholder final : public LayoutBox {
public:
    virtual bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutMultiColumnSpannerPlaceholder || LayoutBox::isOfType(type); }

    static LayoutMultiColumnSpannerPlaceholder* createAnonymous(const LayoutStyle& parentStyle, LayoutBox&);

    LayoutFlowThread* flowThread() const { return toLayoutBlockFlow(parent())->multiColumnFlowThread(); }

    LayoutBox* rendererInFlowThread() const { return m_rendererInFlowThread; }
    void updateMarginProperties();

    virtual const char* name() const override { return "LayoutMultiColumnSpannerPlaceholder"; }

protected:
    virtual void willBeRemovedFromTree() override;
    virtual bool needsPreferredWidthsRecalculation() const override;
    virtual LayoutUnit minPreferredLogicalWidth() const override;
    virtual LayoutUnit maxPreferredLogicalWidth() const override;
    virtual void layout() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual void invalidateTreeIfNeeded(const PaintInvalidationState&) override;
    virtual void paint(const PaintInfo&, const LayoutPoint& paintOffset) override;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

private:
    LayoutMultiColumnSpannerPlaceholder(LayoutBox*);

    LayoutBox* m_rendererInFlowThread; // The actual column-span:all renderer inside the flow thread.
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutMultiColumnSpannerPlaceholder, isLayoutMultiColumnSpannerPlaceholder());

} // namespace blink

#endif
