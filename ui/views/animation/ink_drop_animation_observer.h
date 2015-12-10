// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_OBSERVER_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace views {

// Pure-virtual base class of an observer that can be attached to
// InkDropAnimations.
class VIEWS_EXPORT InkDropAnimationObserver {
 public:
  // Enumeration of the different reasons why an InkDropAnimation has finished.
  enum InkDropAnimationEndedReason {
    // The animation was completed successfully.
    SUCCESS,
    // The animation was stopped prematurely before reaching its final state.
    PRE_EMPTED
  };

  // Notifies the observer that an animation for |ink_drop_state| has started.
  virtual void InkDropAnimationStarted(InkDropState ink_drop_state) = 0;

  // Notifies the observer that an animation for |ink_drop_state| has finished
  // and the reason for completion is given by |reason|. If |reason| is SUCCESS
  // then the animation has progressed to its final frame however if |reason|
  // is |PRE_EMPTED| then the animation was stopped before its final frame. In
  // the event that an animation is in progress for ink drop state 's1' and an
  // animation to a new state 's2' is triggered, then
  // InkDropAnimationEnded(s1, PRE_EMPTED) will be called before
  // InkDropAnimationStarted(s2).
  virtual void InkDropAnimationEnded(InkDropState ink_drop_state,
                                     InkDropAnimationEndedReason reason) = 0;

 protected:
  InkDropAnimationObserver() {}
  virtual ~InkDropAnimationObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropAnimationObserver);
};

// Returns a human readable string for |reason|.  Useful for logging.
std::string ToString(
    InkDropAnimationObserver::InkDropAnimationEndedReason reason);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_OBSERVER_H_
