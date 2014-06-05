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
    CompositingReasons directReasons(const RenderLayer*) const;

    void updateTriggers();

    bool hasOverflowScrollTrigger() const;

    bool requiresCompositingForScrollableFrame() const;
    bool requiresCompositingForPosition(RenderObject*, const RenderLayer*, RenderLayer::ViewportConstrainedNotCompositedReason*) const;

private:
    bool isMainFrame() const;

    CompositingReasons nonStyleDeterminedDirectReasons(const RenderLayer*) const;

    bool requiresCompositingForTransform(RenderObject*) const;
    bool requiresCompositingForFilters(RenderObject*) const;
    bool requiresCompositingForAnimation(RenderObject*) const;
    bool requiresCompositingForPositionSticky(RenderObject*, const RenderLayer*) const;
    bool requiresCompositingForPositionFixed(RenderObject*, const RenderLayer*, RenderLayer::ViewportConstrainedNotCompositedReason*) const;

    RenderView& m_renderView;
    CompositingTriggerFlags m_compositingTriggers;
};

} // namespace WebCore

#endif // CompositingReasonFinder_h
