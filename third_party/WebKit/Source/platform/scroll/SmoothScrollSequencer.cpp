// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scroll/SmoothScrollSequencer.h"

#include "platform/scroll/ProgrammaticScrollAnimator.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

SmoothScrollSequencer::SmoothScrollSequencer() {}

void SmoothScrollSequencer::QueueAnimation(ScrollableArea* scrollable,
                                           ScrollOffset offset) {
  ScrollerAndOffsetPair scroller_offset(scrollable, offset);
  queue_.push_back(scroller_offset);
}

void SmoothScrollSequencer::RunQueuedAnimations() {
  if (queue_.IsEmpty()) {
    current_scrollable_ = nullptr;
    return;
  }
  ScrollerAndOffsetPair scroller_offset = queue_.back();
  queue_.pop_back();
  ScrollableArea* scrollable = scroller_offset.first;
  current_scrollable_ = scrollable;
  ScrollOffset offset = scroller_offset.second;
  scrollable->SetScrollOffset(offset, kSequencedSmoothScroll,
                              kScrollBehaviorSmooth);
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
