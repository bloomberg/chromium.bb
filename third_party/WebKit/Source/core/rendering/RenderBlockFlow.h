/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderBlockFlow_h
#define RenderBlockFlow_h

#include "RenderBlock.h"

namespace WebCore {

class MarginInfo;

class RenderBlockFlow : public RenderBlock {
public:
    explicit RenderBlockFlow(ContainerNode*);
    virtual ~RenderBlockFlow();

    virtual bool isRenderBlockFlow() const OVERRIDE FINAL { return true; }

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) OVERRIDE;
protected:
    // Only used by RenderSVGText, which explicitly overrides RenderBlock::layoutBlock(), do NOT use for anything else.
    void forceLayoutInlineChildren()
    {
        LayoutUnit repaintLogicalTop = 0;
        LayoutUnit repaintLogicalBottom = 0;
        clearFloats();
        layoutInlineChildren(true, repaintLogicalTop, repaintLogicalBottom);
    }

private:
    void layoutBlockChildren(bool relayoutChildren, LayoutUnit& maxFloatLogicalBottom, SubtreeLayoutScope&);
    void layoutInlineChildren(bool relayoutChildren, LayoutUnit& repaintLogicalTop, LayoutUnit& repaintLogicalBottom);

    void layoutBlockChild(RenderBox* child, MarginInfo&, LayoutUnit& previousFloatLogicalBottom, LayoutUnit& maxFloatLogicalBottom);
    void adjustPositionedBlock(RenderBox* child, const MarginInfo&);
    void adjustFloatingBlock(const MarginInfo&);

    void clearFloats();

public:
    class MarginValues {
    public:
        MarginValues(LayoutUnit beforePos, LayoutUnit beforeNeg, LayoutUnit afterPos, LayoutUnit afterNeg)
            : m_positiveMarginBefore(beforePos)
            , m_negativeMarginBefore(beforeNeg)
            , m_positiveMarginAfter(afterPos)
            , m_negativeMarginAfter(afterNeg)
        { }

        LayoutUnit positiveMarginBefore() const { return m_positiveMarginBefore; }
        LayoutUnit negativeMarginBefore() const { return m_negativeMarginBefore; }
        LayoutUnit positiveMarginAfter() const { return m_positiveMarginAfter; }
        LayoutUnit negativeMarginAfter() const { return m_negativeMarginAfter; }

        void setPositiveMarginBefore(LayoutUnit pos) { m_positiveMarginBefore = pos; }
        void setNegativeMarginBefore(LayoutUnit neg) { m_negativeMarginBefore = neg; }
        void setPositiveMarginAfter(LayoutUnit pos) { m_positiveMarginAfter = pos; }
        void setNegativeMarginAfter(LayoutUnit neg) { m_negativeMarginAfter = neg; }

    private:
        LayoutUnit m_positiveMarginBefore;
        LayoutUnit m_negativeMarginBefore;
        LayoutUnit m_positiveMarginAfter;
        LayoutUnit m_negativeMarginAfter;
    };
    MarginValues marginValuesForChild(RenderBox* child) const;

    // Allocated only when some of these fields have non-default values
    struct RenderBlockFlowRareData {
        WTF_MAKE_NONCOPYABLE(RenderBlockFlowRareData); WTF_MAKE_FAST_ALLOCATED;
    public:
        RenderBlockFlowRareData(const RenderBlockFlow* block)
            : m_margins(positiveMarginBeforeDefault(block), negativeMarginBeforeDefault(block), positiveMarginAfterDefault(block), negativeMarginAfterDefault(block))
            , m_discardMarginBefore(false)
            , m_discardMarginAfter(false)
        {
        }

        static LayoutUnit positiveMarginBeforeDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(block->marginBefore(), 0);
        }
        static LayoutUnit negativeMarginBeforeDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(-block->marginBefore(), 0);
        }
        static LayoutUnit positiveMarginAfterDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(block->marginAfter(), 0);
        }
        static LayoutUnit negativeMarginAfterDefault(const RenderBlockFlow* block)
        {
            return std::max<LayoutUnit>(-block->marginAfter(), 0);
        }

        MarginValues m_margins;
        bool m_discardMarginBefore : 1;
        bool m_discardMarginAfter : 1;
    };

protected:
    LayoutUnit maxPositiveMarginBefore() const { return m_rareData ? m_rareData->m_margins.positiveMarginBefore() : RenderBlockFlowRareData::positiveMarginBeforeDefault(this); }
    LayoutUnit maxNegativeMarginBefore() const { return m_rareData ? m_rareData->m_margins.negativeMarginBefore() : RenderBlockFlowRareData::negativeMarginBeforeDefault(this); }
    LayoutUnit maxPositiveMarginAfter() const { return m_rareData ? m_rareData->m_margins.positiveMarginAfter() : RenderBlockFlowRareData::positiveMarginAfterDefault(this); }
    LayoutUnit maxNegativeMarginAfter() const { return m_rareData ? m_rareData->m_margins.negativeMarginAfter() : RenderBlockFlowRareData::negativeMarginAfterDefault(this); }

    void setMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg);
    void setMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg);

    void setMustDiscardMarginBefore(bool = true);
    void setMustDiscardMarginAfter(bool = true);

    bool mustDiscardMarginBefore() const;
    bool mustDiscardMarginAfter() const;

    bool mustDiscardMarginBeforeForChild(const RenderBox*) const;
    bool mustDiscardMarginAfterForChild(const RenderBox*) const;

    bool mustSeparateMarginBeforeForChild(const RenderBox*) const;
    bool mustSeparateMarginAfterForChild(const RenderBox*) const;

    void initMaxMarginValues()
    {
        if (m_rareData) {
            m_rareData->m_margins = MarginValues(RenderBlockFlowRareData::positiveMarginBeforeDefault(this) , RenderBlockFlowRareData::negativeMarginBeforeDefault(this),
                RenderBlockFlowRareData::positiveMarginAfterDefault(this), RenderBlockFlowRareData::negativeMarginAfterDefault(this));

            m_rareData->m_discardMarginBefore = false;
            m_rareData->m_discardMarginAfter = false;
        }
    }

private:
    virtual LayoutUnit collapsedMarginBefore() const OVERRIDE FINAL { return maxPositiveMarginBefore() - maxNegativeMarginBefore(); }
    virtual LayoutUnit collapsedMarginAfter() const OVERRIDE FINAL { return maxPositiveMarginAfter() - maxNegativeMarginAfter(); }

    LayoutUnit collapseMargins(RenderBox* child, MarginInfo&);
    LayoutUnit clearFloatsIfNeeded(RenderBox* child, MarginInfo&, LayoutUnit oldTopPosMargin, LayoutUnit oldTopNegMargin, LayoutUnit yPos);
    LayoutUnit estimateLogicalTopPosition(RenderBox* child, const MarginInfo&, LayoutUnit& estimateWithoutPagination);
    void marginBeforeEstimateForChild(RenderBox*, LayoutUnit&, LayoutUnit&, bool&) const;
    void handleAfterSideOfBlock(LayoutUnit top, LayoutUnit bottom, MarginInfo&);
    void setCollapsedBottomMargin(const MarginInfo&);

    LayoutUnit applyBeforeBreak(RenderBox* child, LayoutUnit logicalOffset); // If the child has a before break, then return a new yPos that shifts to the top of the next page/column.
    LayoutUnit applyAfterBreak(RenderBox* child, LayoutUnit logicalOffset, MarginInfo&); // If the child has an after break, then return a new offset that shifts to the top of the next page/column.

    LayoutUnit adjustBlockChildForPagination(LayoutUnit logicalTopAfterClear, LayoutUnit estimateWithoutPagination, RenderBox* child, bool atBeforeSideOfBlock);

protected:
    OwnPtr<RenderBlockFlowRareData> m_rareData;

    friend class MarginInfo;
};

inline RenderBlockFlow& toRenderBlockFlow(RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderBlockFlow());
    return static_cast<RenderBlockFlow&>(object);
}

inline const RenderBlockFlow& toRenderBlockFlow(const RenderObject& object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(object.isRenderBlockFlow());
    return static_cast<const RenderBlockFlow&>(object);
}

inline RenderBlockFlow* toRenderBlockFlow(RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderBlockFlow());
    return static_cast<RenderBlockFlow*>(object);
}

inline const RenderBlockFlow* toRenderBlockFlow(const RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderBlockFlow());
    return static_cast<const RenderBlockFlow*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderBlockFlow(const RenderBlockFlow*);
void toRenderBlockFlow(const RenderBlockFlow&);

} // namespace WebCore

#endif // RenderBlockFlow_h
