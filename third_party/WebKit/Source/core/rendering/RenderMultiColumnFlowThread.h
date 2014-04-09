/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef RenderMultiColumnFlowThread_h
#define RenderMultiColumnFlowThread_h

#include "core/rendering/RenderFlowThread.h"

namespace WebCore {

class RenderMultiColumnFlowThread FINAL : public RenderFlowThread {
public:
    virtual ~RenderMultiColumnFlowThread();

    static RenderMultiColumnFlowThread* createAnonymous(Document&, RenderStyle* parentStyle);

    RenderBlockFlow* multiColumnBlockFlow() const { return toRenderBlockFlow(parent()); }

    // Populate the flow thread with what's currently its siblings. Called when a regular block
    // becomes a multicol container.
    void populate();

    // Empty the flow thread by moving everything to the parent. Remove all multicol specific
    // renderers. Then destroy the flow thread. Called when a multicol container becomes a regular
    // block.
    void evacuateAndDestroy();

    unsigned columnCount() const { return m_columnCount; }
    LayoutUnit columnWidth() const { return m_columnWidth; }
    LayoutUnit columnHeightAvailable() const { return m_columnHeightAvailable; }
    void setColumnHeightAvailable(LayoutUnit available) { m_columnHeightAvailable = available; }
    bool requiresBalancing() const { return !columnHeightAvailable() || multiColumnBlockFlow()->style()->columnFill() == ColumnFillBalance; }

    void layoutColumns(bool relayoutChildren, SubtreeLayoutScope&);
    bool computeColumnCountAndWidth();
    bool recalculateColumnHeights();

private:
    RenderMultiColumnFlowThread();

    virtual const char* renderName() const OVERRIDE;
    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const OVERRIDE;
    virtual void autoGenerateRegionsToBlockOffset(LayoutUnit) OVERRIDE;
    virtual LayoutUnit initialLogicalWidth() const OVERRIDE;
    virtual void setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage) OVERRIDE;
    virtual void updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight) OVERRIDE;
    virtual bool addForcedRegionBreak(LayoutUnit, RenderObject* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0) OVERRIDE;
    virtual bool isPageLogicalHeightKnown() const OVERRIDE;

    unsigned m_columnCount; // The used value of column-count
    LayoutUnit m_columnWidth; // The used value of column-width
    LayoutUnit m_columnHeightAvailable; // Total height available to columns, or 0 if auto.
    bool m_inBalancingPass; // Set when relayouting for column balancing.
    bool m_needsRebalancing;
};

} // namespace WebCore

#endif // RenderMultiColumnFlowThread_h

