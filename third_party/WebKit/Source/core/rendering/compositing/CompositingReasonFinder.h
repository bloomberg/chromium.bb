// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasonFinder_h
#define CompositingReasonFinder_h

#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/CompositingTriggers.h"
#include "platform/graphics/CompositingReasons.h"

namespace WebCore {

class RenderLayer;
class RenderObject;
class RenderView;

class CompositingReasonFinder {
    WTF_MAKE_NONCOPYABLE(CompositingReasonFinder);
public:
    explicit CompositingReasonFinder(RenderView&);

    CompositingReasons styleDeterminedReasons(RenderObject*) const;
    CompositingReasons directReasons(const RenderLayer*, bool* needToRecomputeCompositingRequirements = 0) const;

    void updateTriggers();

    bool hasOverflowScrollTrigger() const;
    bool hasLegacyOverflowScrollTrigger() const;

    bool requiresCompositingForScrollableFrame() const;
    bool requiresCompositingForPosition(RenderObject*, const RenderLayer*, RenderLayer::ViewportConstrainedNotCompositedReason*, bool* needToRecomputeCompositingRequirements) const;

private:
    bool isMainFrame() const;

    CompositingReasons nonStyleDeterminedDirectReasons(const RenderLayer*, bool* needToRecomputeCompositingRequirements) const;

    bool requiresCompositingForAnimation(RenderObject*) const;
    bool requiresCompositingForTransform(RenderObject*) const;
    bool requiresCompositingForBackfaceVisibilityHidden(RenderObject*) const;
    bool requiresCompositingForFilters(RenderObject*) const;
    bool requiresCompositingForOverflowScrollingParent(const RenderLayer*) const;
    bool requiresCompositingForOutOfFlowClipping(const RenderLayer*) const;
    bool requiresCompositingForOverflowScrolling(const RenderLayer*) const;
    bool requiresCompositingForPositionSticky(RenderObject*, const RenderLayer*) const;
    bool requiresCompositingForPositionFixed(RenderObject*, const RenderLayer*, RenderLayer::ViewportConstrainedNotCompositedReason*, bool* needToRecomputeCompositingRequirements) const;
    bool requiresCompositingForWillChangeCompositingHint(const RenderObject*) const;

    RenderView& m_renderView;
    CompositingTriggerFlags m_compositingTriggers;
};

} // namespace WebCore

#endif // CompositingReasonFinder_h
