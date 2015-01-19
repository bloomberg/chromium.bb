// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderMultiColumnSpannerPlaceholder_h
#define RenderMultiColumnSpannerPlaceholder_h

#include "core/rendering/RenderMultiColumnFlowThread.h"

namespace blink {

// Placeholder renderer for column-span:all elements. The column-span:all renderer itself is a
// descendant of the flow thread, but due to its out-of-flow nature, we need something on the
// outside to take care of its positioning and sizing. RenderMultiColumnSpannerPlaceholder objects
// are siblings of RenderMultiColumnSet objects, i.e. direct children of the multicol container.
class RenderMultiColumnSpannerPlaceholder final : public RenderBox {
public:
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectRenderMultiColumnSpannerPlaceholder || RenderBox::isOfType(type); }

    static RenderMultiColumnSpannerPlaceholder* createAnonymous(RenderStyle* parentStyle, RenderBox*);

    RenderFlowThread* flowThread() const { return toRenderBlockFlow(parent())->multiColumnFlowThread(); }

    RenderBox* rendererInFlowThread() const { return m_rendererInFlowThread; }
    void setRendererInFlowThread(RenderBox* renderer) { m_rendererInFlowThread = renderer; }
    void spannerWillBeRemoved();

protected:
    virtual void willBeRemovedFromTree() override;
    virtual bool needsPreferredWidthsRecalculation() const override;
    virtual void layout() override;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;
    virtual void invalidateTreeIfNeeded(const PaintInvalidationState&) override;
    virtual void paint(const PaintInfo&, const LayoutPoint& paintOffset) override;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
    virtual const char* renderName() const override;

private:
    RenderMultiColumnSpannerPlaceholder(RenderBox*);

    RenderBox* m_rendererInFlowThread; // The actual column-span:all renderer inside the flow thread.
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderMultiColumnSpannerPlaceholder, isRenderMultiColumnSpannerPlaceholder());

} // namespace blink

#endif
