/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollAnimatorMac_h
#define ScrollAnimatorMac_h

#include <memory>
#include "platform/Timer.h"
#include "platform/WebTaskRunner.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/wtf/RetainPtr.h"

OBJC_CLASS BlinkScrollAnimationHelperDelegate;
OBJC_CLASS BlinkScrollbarPainterControllerDelegate;
OBJC_CLASS BlinkScrollbarPainterDelegate;

typedef id ScrollbarPainterController;

namespace blink {

class Scrollbar;

class PLATFORM_EXPORT ScrollAnimatorMac : public ScrollAnimatorBase {
  USING_PRE_FINALIZER(ScrollAnimatorMac, Dispose);

 public:
  ScrollAnimatorMac(ScrollableArea*);
  ~ScrollAnimatorMac() override;

  void Dispose() override;

  void ImmediateScrollToOffsetForScrollAnimation(
      const ScrollOffset& new_offset);
  bool HaveScrolledSincePageLoad() const {
    return have_scrolled_since_page_load_;
  }

  void UpdateScrollerStyle();

  bool ScrollbarPaintTimerIsActive() const;
  void StartScrollbarPaintTimer();
  void StopScrollbarPaintTimer();

  void SendContentAreaScrolledSoon(const ScrollOffset& scroll_delta);

  void SetVisibleScrollerThumbRect(const IntRect&);

  DEFINE_INLINE_VIRTUAL_TRACE() { ScrollAnimatorBase::Trace(visitor); }

 private:
  RetainPtr<id> scroll_animation_helper_;
  RetainPtr<BlinkScrollAnimationHelperDelegate>
      scroll_animation_helper_delegate_;

  RetainPtr<ScrollbarPainterController> scrollbar_painter_controller_;
  RetainPtr<BlinkScrollbarPainterControllerDelegate>
      scrollbar_painter_controller_delegate_;
  RetainPtr<BlinkScrollbarPainterDelegate>
      horizontal_scrollbar_painter_delegate_;
  RetainPtr<BlinkScrollbarPainterDelegate> vertical_scrollbar_painter_delegate_;

  void InitialScrollbarPaintTask();
  TaskHandle initial_scrollbar_paint_task_handle_;

  void SendContentAreaScrolledTask();
  TaskHandle send_content_area_scrolled_task_handle_;
  RefPtr<WebTaskRunner> task_runner_;
  ScrollOffset content_area_scrolled_timer_scroll_delta_;

  ScrollResult UserScroll(ScrollGranularity,
                          const ScrollOffset& delta) override;
  void ScrollToOffsetWithoutAnimation(const ScrollOffset&) override;

  void CancelAnimation() override;

  void ContentAreaWillPaint() const override;
  void MouseEnteredContentArea() const override;
  void MouseExitedContentArea() const override;
  void MouseMovedInContentArea() const override;
  void MouseEnteredScrollbar(Scrollbar&) const override;
  void MouseExitedScrollbar(Scrollbar&) const override;
  void ContentsResized() const override;
  void ContentAreaDidShow() const override;
  void ContentAreaDidHide() const override;

  void FinishCurrentScrollAnimations() override;

  void DidAddVerticalScrollbar(Scrollbar&) override;
  void WillRemoveVerticalScrollbar(Scrollbar&) override;
  void DidAddHorizontalScrollbar(Scrollbar&) override;
  void WillRemoveHorizontalScrollbar(Scrollbar&) override;

  void NotifyContentAreaScrolled(const ScrollOffset& delta) override;

  bool SetScrollbarsVisibleForTesting(bool) override;

  ScrollOffset AdjustScrollOffsetIfNecessary(const ScrollOffset&) const;

  void ImmediateScrollTo(const ScrollOffset&);

  bool have_scrolled_since_page_load_;
  bool needs_scroller_style_update_;
  IntRect visible_scroller_thumb_rect_;
};

}  // namespace blink

#endif  // ScrollAnimatorMac_h
