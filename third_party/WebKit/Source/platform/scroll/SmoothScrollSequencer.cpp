// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/SmoothScrollSequencer.h"

#include "platform/scroll/ProgrammaticScrollAnimator.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

DEFINE_TRACE(SequencedScroll) {
  visitor->Trace(scrollable_area);
}

void SmoothScrollSequencer::QueueAnimation(ScrollableArea* scrollable,
                                           ScrollOffset offset,
                                           ScrollBehavior behavior) {
  if (scrollable->ClampScrollOffset(offset) != scrollable->GetScrollOffset())
    queue_.push_back(new SequencedScroll(scrollable, offset, behavior));
}

void SmoothScrollSequencer::RunQueuedAnimations() {
  if (queue_.IsEmpty()) {
    current_scrollable_ = nullptr;
    return;
  }
  SequencedScroll* sequenced_scroll = queue_.back();
  queue_.pop_back();
  current_scrollable_ = sequenced_scroll->scrollable_area;
  current_scrollable_->SetScrollOffset(sequenced_scroll->scroll_offset,
                                       kSequencedScroll,
                                       sequenced_scroll->scroll_behavior);
}

void SmoothScrollSequencer::AbortAnimations() {
  if (current_scrollable_) {
    current_scrollable_->CancelProgrammaticScrollAnimation();
    current_scrollable_ = nullptr;
  }
  queue_.clear();
}

DEFINE_TRACE(SmoothScrollSequencer) {
  visitor->Trace(queue_);
  visitor->Trace(current_scrollable_);
}

}  // namespace blink
