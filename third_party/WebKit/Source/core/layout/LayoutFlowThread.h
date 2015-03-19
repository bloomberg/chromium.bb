/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LayoutFlowThread_h
#define LayoutFlowThread_h

#include "core/layout/LayoutBlockFlow.h"
#include "core/paint/DeprecatedPaintLayerFragment.h"
#include "wtf/ListHashSet.h"

namespace blink {

class LayoutMultiColumnSet;
class LayoutRegion;

typedef ListHashSet<LayoutMultiColumnSet*> LayoutMultiColumnSetList;

// LayoutFlowThread is used to collect all the render objects that participate in a
// flow thread. It will also help in doing the layout. However, it will not render
// directly to screen. Instead, LayoutRegion objects will redirect their paint
// and nodeAtPoint methods to this object. Each LayoutRegion will actually be a viewPort
// of the LayoutFlowThread.

class LayoutFlowThread: public LayoutBlockFlow {
public:
    LayoutFlowThread();
    virtual ~LayoutFlowThread() { };

    virtual bool isLayoutFlowThread() const override final { return true; }
    virtual bool isLayoutMultiColumnFlowThread() const { return false; }
    virtual bool isLayoutPagedFlowThread() const { return false; }

    virtual bool supportsPaintInvalidationStateCachedOffsets() const override { return false; }

    virtual void layout() override;

    // Always create a Layer for the LayoutFlowThread so that we
    // can easily avoid drawing the children directly.
    virtual DeprecatedPaintLayerType layerTypeRequired() const override final { return NormalDeprecatedPaintLayer; }

    // Skip past a column spanner during flow thread layout. Spanners are not laid out inside the
    // flow thread, since the flow thread is not in a spanner's containing block chain (since the
    // containing block is the multicol container). If the spanner follows right after a column set
    // (as opposed to following another spanner), we may have to stretch the flow thread to ensure
    // completely filled columns in the preceding column set. Return this adjustment, if any.
    virtual LayoutUnit skipColumnSpanner(LayoutBox*, LayoutUnit logicalTopInFlowThread) { return LayoutUnit(); }

    virtual void flowThreadDescendantWasInserted(LayoutObject*) { }
    virtual void flowThreadDescendantWillBeRemoved(LayoutObject*) { }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override final;

    virtual void addRegionToThread(LayoutMultiColumnSet*) = 0;
    virtual void removeRegionFromThread(LayoutMultiColumnSet*);

    virtual void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    bool hasRegions() const { return m_multiColumnSetList.size(); }

    void validateRegions();
    void invalidateRegions();
    bool hasValidRegionInfo() const { return !m_regionsInvalidated && !m_multiColumnSetList.isEmpty(); }

    virtual void mapRectToPaintInvalidationBacking(const LayoutBoxModelObject* paintInvalidationContainer, LayoutRect&, const PaintInvalidationState*) const override;

    LayoutUnit pageLogicalHeightForOffset(LayoutUnit);
    LayoutUnit pageRemainingLogicalHeightForOffset(LayoutUnit, PageBoundaryRule = IncludePageBoundary);

    virtual void setPageBreak(LayoutUnit /*offset*/, LayoutUnit /*spaceShortage*/) { }
    virtual void updateMinimumPageHeight(LayoutUnit /*offset*/, LayoutUnit /*minHeight*/) { }

    bool regionsHaveUniformLogicalHeight() const { return m_regionsHaveUniformLogicalHeight; }

    // FIXME: These 2 functions should return a LayoutMultiColumnSet.
    LayoutRegion* firstRegion() const;
    LayoutRegion* lastRegion() const;

    virtual bool addForcedRegionBreak(LayoutUnit, LayoutObject* breakChild, bool isBefore, LayoutUnit* offsetBreakAdjustment = 0) { return false; }

    virtual bool isPageLogicalHeightKnown() const { return true; }
    bool pageLogicalSizeChanged() const { return m_pageLogicalSizeChanged; }

    void collectLayerFragments(DeprecatedPaintLayerFragments&, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect);

    // Return the visual bounding box based on the supplied flow-thread bounding box. Both
    // rectangles are completely physical in terms of writing mode.
    LayoutRect fragmentsBoundingBox(const LayoutRect& layerBoundingBox) const;

    LayoutPoint flowThreadPointToVisualPoint(const LayoutPoint& flowThreadPoint) const
    {
        return flowThreadPoint + columnOffset(flowThreadPoint);
    }

    // Used to estimate the maximum height of the flow thread.
    static LayoutUnit maxLogicalHeight() { return LayoutUnit::max() / 2; }

    virtual LayoutMultiColumnSet* columnSetAtBlockOffset(LayoutUnit) const = 0;

    virtual const char* name() const = 0;

protected:
    void generateColumnSetIntervalTree();

    LayoutMultiColumnSetList m_multiColumnSetList;

    typedef PODInterval<LayoutUnit, LayoutMultiColumnSet*> MultiColumnSetInterval;
    typedef PODIntervalTree<LayoutUnit, LayoutMultiColumnSet*> MultiColumnSetIntervalTree;

    class MultiColumnSetSearchAdapter {
    public:
        MultiColumnSetSearchAdapter(LayoutUnit offset)
            : m_offset(offset)
            , m_result(0)
        {
        }

        const LayoutUnit& lowValue() const { return m_offset; }
        const LayoutUnit& highValue() const { return m_offset; }
        void collectIfNeeded(const MultiColumnSetInterval&);

        LayoutMultiColumnSet* result() const { return m_result; }

    private:
        LayoutUnit m_offset;
        LayoutMultiColumnSet* m_result;
    };

    MultiColumnSetIntervalTree m_multiColumnSetIntervalTree;

    bool m_regionsInvalidated : 1;
    bool m_regionsHaveUniformLogicalHeight : 1;
    bool m_pageLogicalSizeChanged : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFlowThread, isLayoutFlowThread());

// These structures are used by PODIntervalTree for debugging.
#ifndef NDEBUG
template <> struct ValueToString<LayoutUnit> {
    static String string(const LayoutUnit value) { return String::number(value.toFloat()); }
};

template <> struct ValueToString<LayoutMultiColumnSet*> {
    static String string(const LayoutMultiColumnSet* value) { return String::format("%p", value); }
};
#endif

} // namespace blink

#endif // LayoutFlowThread_h
