// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollAnimatorCompositorCoordinator_h
#define ScrollAnimatorCompositorCoordinator_h

#include "platform/PlatformExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCompositorAnimationDelegate.h"
#include "public/platform/WebCompositorAnimationPlayerClient.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ScrollableArea;
class WebCompositorAnimationPlayer;
class WebCompositorAnimationTimeline;

class PLATFORM_EXPORT ScrollAnimatorCompositorCoordinator : public NoBaseWillBeGarbageCollectedFinalized<ScrollAnimatorCompositorCoordinator>, private WebCompositorAnimationPlayerClient, WebCompositorAnimationDelegate {
public:
    virtual ~ScrollAnimatorCompositorCoordinator();

    bool hasAnimationThatRequiresService() const;
    void cancelAnimation();

    virtual void resetAnimationState();

    virtual ScrollableArea* scrollableArea() const = 0;
    virtual void tickAnimation(double monotonicTime) = 0;
    virtual void updateCompositorAnimations() = 0;
    virtual void notifyCompositorAnimationFinished(int groupId) = 0;
    virtual void layerForCompositedScrollingDidChange(WebCompositorAnimationTimeline*) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    explicit ScrollAnimatorCompositorCoordinator();

    bool addAnimation(PassOwnPtr<WebCompositorAnimation>);
    void removeAnimation();

    void compositorAnimationFinished(int groupId);
    void reattachCompositorPlayerIfNeeded(WebCompositorAnimationTimeline*);

    // WebCompositorAnimationDelegate implementation.
    void notifyAnimationStarted(double monotonicTime, int group) override;
    void notifyAnimationFinished(double monotonicTime, int group) override;

    // WebCompositorAnimationPlayerClient implementation.
    WebCompositorAnimationPlayer* compositorPlayer() const override;

    enum class RunState {
        // No animation.
        Idle,

        // Waiting to send an animation to the compositor. There might also
        // already be another animation running on the compositor that will need
        // to be canceled first.
        WaitingToSendToCompositor,

        // Running an animation on the compositor.
        RunningOnCompositor,

        // Running an animation on the main thread.
        RunningOnMainThread,

        // Waiting to cancel the animation currently running on the compositor.
        // There is no pending animation to replace the canceled animation.
        WaitingToCancelOnCompositor
    };

    OwnPtr<WebCompositorAnimationPlayer> m_compositorPlayer;
    int m_compositorAnimationAttachedToLayerId;
    RunState m_runState;
    int m_compositorAnimationId;
    int m_compositorAnimationGroupId;
};

} // namespace blink

#endif // ScrollAnimatorCompositorCoordinator_h
