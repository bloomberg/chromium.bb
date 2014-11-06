// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderMultiColumnSpannerSet_h
#define RenderMultiColumnSpannerSet_h

#include "core/rendering/RenderMultiColumnSet.h"

namespace blink {

// "Column" set for column-span:all elements. Positions the portion of the flow thread that a
// spanner takes up. The actual renderer that sets column-span all is laid out as part of flow
// thread layout (although it cannot use the flow thread width when calculating its own with; it has
// to pretend that the containing block is the multicol container, not the flow
// thread). RenderMultiColumnSpannerSet just makes sure that it's positioned correctly among other
// column sets, within the multicol container.
//
// FIXME: should consider not inheriting from RenderMultiColumnSet (but rather from some common base
// class). Most of what's in RenderMultiColumnSet is of no interest to us.
class RenderMultiColumnSpannerSet final : public RenderMultiColumnSet {
public:
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectRenderMultiColumnSpannerSet || RenderMultiColumnSet::isOfType(type); }

    static RenderMultiColumnSpannerSet* createAnonymous(RenderMultiColumnFlowThread*, RenderStyle* parentStyle, RenderBox*);

    RenderBox* rendererInFlowThread() const { return m_rendererInFlowThread; }

protected:
    virtual bool recalculateColumnHeight(BalancedHeightCalculation) override;
    virtual const char* renderName() const override;

private:
    RenderMultiColumnSpannerSet(RenderMultiColumnFlowThread*, RenderBox*);

    RenderBox* m_rendererInFlowThread; // The actual column-span:all renderer inside the flow thread.
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderMultiColumnSpannerSet, isRenderMultiColumnSpannerSet());

} // namespace blink

#endif
