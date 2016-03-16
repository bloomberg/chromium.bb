// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollAnimatorCompositorCoordinator_h
#define ScrollAnimatorCompositorCoordinator_h

#include "base/gtest_prod_util.h"
#include "cc/animation/animation_curve.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ScrollableArea;
class CompositorAnimation;
class CompositorAnimationPlayer;
class CompositorAnimationTimeline;

class PLATFORM_EXPORT ScrollAnimatorCompositorCoordinator : public NoBaseWillBeGarbageCollectedFinalized<ScrollAnimatorCompositorCoordinator>, private CompositorAnimationPlayerClient, CompositorAnimationDelegate {
    USING_FAST_MALLOC_WILL_BE_REMOVED(ScrollAnimatorCompositorCoordinator);
    WTF_MAKE_NONCOPYABLE(ScrollAnimatorCompositorCoordinator);
    WILL_BE_USING_PRE_FINALIZER(ScrollAnimatorCompositorCoordinator, dispose);
public:
    virtual ~ScrollAnimatorCompositorCoordinator();

    bool hasAnimationThatRequiresService() const;
    void dispose();
    String runStateAsText() const;

    virtual bool hasRunningAnimation() const { return false; }

    virtual void resetAnimationState();
    virtual void cancelAnimation();
    // Aborts the currently running scroll offset animation on the compositor
    // and continues it on the main thread. This should only be called when in
    // DocumentLifecycle::LifecycleState::CompositingClean state.
    virtual void takeoverCompositorAnimation();

    virtual ScrollableArea* getScrollableArea() const = 0;
    virtual void tickAnimation(double monotonicTime) = 0;
    virtual void updateCompositorAnimations() = 0;
    virtual void notifyCompositorAnimationFinished(int groupId) = 0;
    virtual void notifyCompositorAnimationAborted(int groupId) = 0;
    virtual void layerForCompositedScrollingDidChange(CompositorAnimationTimeline*) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    explicit ScrollAnimatorCompositorCoordinator();

    bool addAnimation(PassOwnPtr<CompositorAnimation>);
    void removeAnimation();
    virtual void abortAnimation();

    FloatPoint compositorOffsetFromBlinkOffset(FloatPoint);
    FloatPoint blinkOffsetFromCompositorOffset(FloatPoint);

    void compositorAnimationFinished(int groupId);
    // Returns true if the compositor player was attached to a new layer.
    bool reattachCompositorPlayerIfNeeded(CompositorAnimationTimeline*);

    // CompositorAnimationDelegate implementation.
    void notifyAnimationStarted(double monotonicTime, int group) override;
    void notifyAnimationFinished(double monotonicTime, int group) override;
    void notifyAnimationAborted(double monotonicTime, int group) override;
    void notifyAnimationTakeover(
        double monotonicTime,
        double animationStartTime,
        scoped_ptr<cc::AnimationCurve>) override { };

    // CompositorAnimationPlayerClient implementation.
    CompositorAnimationPlayer* compositorPlayer() const override;

    friend class Internals;
    FRIEND_TEST_ALL_PREFIXES(ScrollAnimatorTest, MainThreadStates);
    FRIEND_TEST_ALL_PREFIXES(ScrollAnimatorTest, AnimatedScrollTakeover);

    enum class RunState {
        // No animation.
        Idle,

        // Waiting to send an animation to the compositor. There might also
        // already be another animation running on the compositor that will need
        // to be canceled first.
        WaitingToSendToCompositor,

        // Running an animation on the compositor.
        RunningOnCompositor,

        // Running an animation on the compositor but needs update.
        RunningOnCompositorButNeedsUpdate,

        // Running an animation on the main thread.
        RunningOnMainThread,

        // Waiting to cancel the animation currently running on the compositor.
        // There is no pending animation to replace the canceled animation.
        WaitingToCancelOnCompositor,

        // Finished an animation that was running on the main thread or the
        // compositor thread. When in this state, post animation cleanup can
        // be performed.
        PostAnimationCleanup,

        // Running an animation on the compositor but need to continue it
        // on the main thread. This could happen if a main thread scrolling
        // reason is added while animating the scroll offset.
        RunningOnCompositorButNeedsTakeover,
    };

    OwnPtr<CompositorAnimationPlayer> m_compositorPlayer;
    int m_compositorAnimationAttachedToLayerId;
    RunState m_runState;
    int m_compositorAnimationId;
    int m_compositorAnimationGroupId;
};

} // namespace blink

#endif // ScrollAnimatorCompositorCoordinator_h
