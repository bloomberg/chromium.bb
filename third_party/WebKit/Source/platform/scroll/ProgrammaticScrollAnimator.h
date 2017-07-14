// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ProgrammaticScrollAnimator_h
#define ProgrammaticScrollAnimator_h

#include <memory>
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollAnimatorCompositorCoordinator.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class ScrollableArea;
class CompositorAnimationTimeline;
class CompositorScrollOffsetAnimationCurve;

// Animator for fixed-destination scrolls, such as those triggered by
// CSSOM View scroll APIs.
class ProgrammaticScrollAnimator : public ScrollAnimatorCompositorCoordinator {
  WTF_MAKE_NONCOPYABLE(ProgrammaticScrollAnimator);

 public:
  static ProgrammaticScrollAnimator* Create(ScrollableArea* scrollable_area) {
    return new ProgrammaticScrollAnimator(scrollable_area);
  }

  virtual ~ProgrammaticScrollAnimator();

  void ScrollToOffsetWithoutAnimation(const ScrollOffset&,
                                      bool is_sequenced_scroll);
  void AnimateToOffset(const ScrollOffset&, bool is_sequenced_scroll = false);

  // ScrollAnimatorCompositorCoordinator implementation.
  void ResetAnimationState() override;
  void CancelAnimation() override;
  void TakeOverCompositorAnimation() override {}
  ScrollableArea* GetScrollableArea() const override {
    return scrollable_area_;
  }
  void TickAnimation(double monotonic_time) override;
  void UpdateCompositorAnimations() override;
  void NotifyCompositorAnimationFinished(int group_id) override;
  void NotifyCompositorAnimationAborted(int group_id) override {}
  void LayerForCompositedScrollingDidChange(
      CompositorAnimationTimeline*) override;

  DECLARE_TRACE();

 private:
  explicit ProgrammaticScrollAnimator(ScrollableArea*);

  void NotifyOffsetChanged(const ScrollOffset&);
  void AnimationFinished();

  Member<ScrollableArea> scrollable_area_;
  std::unique_ptr<CompositorScrollOffsetAnimationCurve> animation_curve_;
  ScrollOffset target_offset_;
  double start_time_;
  // is_sequenced_scroll_ is true for the entire duration of an animated scroll
  // as well as during an instant scroll if that scroll is part of a sequence.
  // It resets to false at the end of the scroll. It controls whether we should
  // abort the smooth scroll sequence after an instant SetScrollOffset.
  bool is_sequenced_scroll_;
};

}  // namespace blink

#endif  // ProgrammaticScrollAnimator_h
