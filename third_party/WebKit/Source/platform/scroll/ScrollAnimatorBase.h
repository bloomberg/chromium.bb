/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef ScrollAnimatorBase_h
#define ScrollAnimatorBase_h

#include "platform/PlatformExport.h"
#include "platform/PlatformWheelEvent.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Forward.h"

namespace blink {

class ScrollableArea;
class Scrollbar;
class WebCompositorAnimationTimeline;

class PLATFORM_EXPORT ScrollAnimatorBase : public ScrollAnimatorCompositorCoordinator {
public:
    static PassOwnPtrWillBeRawPtr<ScrollAnimatorBase> create(ScrollableArea*);

    virtual ~ScrollAnimatorBase();

    virtual void dispose() { }

    // Computes a scroll destination for the given parameters.  The returned
    // ScrollResultOneDimensional will have didScroll set to false if already at
    // the destination.  Otherwise, starts scrolling towards the destination and
    // didScroll is true.  Scrolling may be immediate or animated. The base
    // class implementation always scrolls immediately, never animates.
    virtual ScrollResultOneDimensional userScroll(ScrollbarOrientation, ScrollGranularity, float step, float delta);

    virtual void scrollToOffsetWithoutAnimation(const FloatPoint&);

    virtual void setIsActive() { }

#if OS(MACOSX)
    virtual void handleWheelEventPhase(PlatformWheelEventPhase) { }
#endif

    void setCurrentPosition(const FloatPoint&);
    FloatPoint currentPosition() const;
    virtual FloatPoint desiredTargetPosition() const { return currentPosition(); }

    // Returns how much of pixelDelta will be used by the underlying scrollable
    // area.
    virtual float computeDeltaToConsume(ScrollbarOrientation, float pixelDelta) const;


    // ScrollAnimatorCompositorCoordinator implementation.
    ScrollableArea* scrollableArea() const override { return m_scrollableArea; }
    void tickAnimation(double monotonicTime) override { };
    void cancelAnimation() override { }
    void updateCompositorAnimations() override { };
    void notifyCompositorAnimationFinished(int groupId) override { };
    void notifyCompositorAnimationAborted(int groupId) override { };
    void layerForCompositedScrollingDidChange(WebCompositorAnimationTimeline*) override { };

    virtual void contentAreaWillPaint() const { }
    virtual void mouseEnteredContentArea() const { }
    virtual void mouseExitedContentArea() const { }
    virtual void mouseMovedInContentArea() const { }
    virtual void mouseEnteredScrollbar(Scrollbar&) const { }
    virtual void mouseExitedScrollbar(Scrollbar&) const { }
    virtual void willStartLiveResize() { }
    virtual void updateAfterLayout() { }
    virtual void contentsResized() const { }
    virtual void willEndLiveResize() { }
    virtual void contentAreaDidShow() const { }
    virtual void contentAreaDidHide() const { }

    virtual void finishCurrentScrollAnimations() { }

    virtual void didAddVerticalScrollbar(Scrollbar&) { }
    virtual void willRemoveVerticalScrollbar(Scrollbar&) { }
    virtual void didAddHorizontalScrollbar(Scrollbar&) { }
    virtual void willRemoveHorizontalScrollbar(Scrollbar&) { }

    virtual bool shouldScrollbarParticipateInHitTesting(Scrollbar&) { return true; }

    virtual void notifyContentAreaScrolled(const FloatSize&) { }

    virtual bool setScrollbarsVisibleForTesting(bool) { return false; }

    DECLARE_VIRTUAL_TRACE();

protected:
    explicit ScrollAnimatorBase(ScrollableArea*);

    virtual void notifyPositionChanged();

    float clampScrollPosition(ScrollbarOrientation, float) const;

    RawPtrWillBeMember<ScrollableArea> m_scrollableArea;
    float m_currentPosX; // We avoid using a FloatPoint in order to reduce
    float m_currentPosY; // subclass code complexity.
};

} // namespace blink

#endif // ScrollAnimatorBase_h
