// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ProgrammaticScrollAnimator_h
#define ProgrammaticScrollAnimator_h

#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class ScrollableArea;
class WebCompositorAnimationTimeline;
class WebScrollOffsetAnimationCurve;

// Animator for fixed-destination scrolls, such as those triggered by
// CSSOM View scroll APIs.
class ProgrammaticScrollAnimator : public ScrollAnimatorCompositorCoordinator {
    WTF_MAKE_NONCOPYABLE(ProgrammaticScrollAnimator);
    USING_FAST_MALLOC_WILL_BE_REMOVED(ProgrammaticScrollAnimator);
public:
    static PassOwnPtrWillBeRawPtr<ProgrammaticScrollAnimator> create(ScrollableArea*);

    virtual ~ProgrammaticScrollAnimator();

    void scrollToOffsetWithoutAnimation(const FloatPoint&);
    void animateToOffset(FloatPoint);

    // ScrollAnimatorCompositorCoordinator implementation.
    void resetAnimationState() override;
    void cancelAnimation() override;
    ScrollableArea* scrollableArea() const override { return m_scrollableArea; }
    void tickAnimation(double monotonicTime) override;
    void updateCompositorAnimations() override;
    void notifyCompositorAnimationFinished(int groupId) override;
    void notifyCompositorAnimationAborted(int groupId) override { };
    void layerForCompositedScrollingDidChange(WebCompositorAnimationTimeline*) override;

    DECLARE_TRACE();

private:
    explicit ProgrammaticScrollAnimator(ScrollableArea*);

    void notifyPositionChanged(const DoublePoint&);

    RawPtrWillBeMember<ScrollableArea> m_scrollableArea;
    OwnPtr<WebScrollOffsetAnimationCurve> m_animationCurve;
    FloatPoint m_targetOffset;
    double m_startTime;
};

} // namespace blink

#endif // ProgrammaticScrollAnimator_h
