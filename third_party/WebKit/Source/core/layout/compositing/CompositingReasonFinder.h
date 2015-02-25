// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasonFinder_h
#define CompositingReasonFinder_h

#include "core/layout/Layer.h"
#include "core/layout/compositing/CompositingTriggers.h"
#include "platform/graphics/CompositingReasons.h"

namespace blink {

class LayoutObject;
class LayoutView;

class CompositingReasonFinder {
    WTF_MAKE_NONCOPYABLE(CompositingReasonFinder);
public:
    explicit CompositingReasonFinder(LayoutView&);

    CompositingReasons potentialCompositingReasonsFromStyle(LayoutObject*) const;
    CompositingReasons directReasons(const Layer*) const;

    void updateTriggers();

    bool hasOverflowScrollTrigger() const;
    bool requiresCompositingForScrollableFrame() const;

private:
    bool isMainFrame() const;

    CompositingReasons nonStyleDeterminedDirectReasons(const Layer*) const;

    bool requiresCompositingForTransform(LayoutObject*) const;
    bool requiresCompositingForAnimation(const LayoutStyle&) const;
    bool requiresCompositingForPositionFixed(const Layer*) const;
    bool requiresCompositingForScrollBlocksOn(const LayoutObject*) const;

    LayoutView& m_layoutView;
    CompositingTriggerFlags m_compositingTriggers;
};

} // namespace blink

#endif // CompositingReasonFinder_h
