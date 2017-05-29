// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SmoothScrollSequencer_h
#define SmoothScrollSequencer_h

#include <utility>
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class ScrollableArea;

// A sequencer that queues the nested scrollers from inside to outside,
// so that they can be animated from outside to inside when smooth scroll
// is called.
class PLATFORM_EXPORT SmoothScrollSequencer final
    : public GarbageCollected<SmoothScrollSequencer> {
 public:
  SmoothScrollSequencer();
  // Add a scroll offset animation to the back of a queue.
  void QueueAnimation(ScrollableArea*, ScrollOffset);

  // Run the animation at the back of the queue.
  void RunQueuedAnimations();

  // Abort the currently running animation and all the animations in the queue.
  void AbortAnimations();

  DECLARE_TRACE();

 private:
  typedef std::pair<Member<ScrollableArea>, ScrollOffset> ScrollerAndOffsetPair;
  HeapVector<ScrollerAndOffsetPair> queue_;
  Member<ScrollableArea> current_scrollable_;
};

}  // namespace blink

#endif  // SmoothScrollSequencer_h
