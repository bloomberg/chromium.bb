// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_H_
#define UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop_animation_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {

// Simple base class for animations that provide visual feedback for View state.
// Manages the attached InkDropAnimationObservers.
//
// TODO(bruthig): Document the ink drop ripple on chromium.org and add a link to
// the doc here.
class VIEWS_EXPORT InkDropAnimation {
 public:
  // The opacity of the ink drop when it is not visible.
  static const float kHiddenOpacity;

  // The opacity of the ink drop when it is visible.
  static const float kVisibleOpacity;

  InkDropAnimation();
  virtual ~InkDropAnimation();

  void AddObserver(InkDropAnimationObserver* observer);
  void RemoveObserver(InkDropAnimationObserver* observer);

  // Gets the target InkDropState, i.e. the last |ink_drop_state| passed to
  // AnimateToState() or set by HideImmediately().
  virtual InkDropState GetTargetInkDropState() const = 0;

  // Animates from the current InkDropState to the new |ink_drop_state|.
  //
  // NOTE: GetTargetInkDropState() should return the new |ink_drop_state| value
  // to any observers being notified as a result of the call.
  virtual void AnimateToState(InkDropState ink_drop_state) = 0;

  // The root Layer that can be added in to a Layer tree.
  virtual ui::Layer* GetRootLayer() = 0;

  // Returns true when the ripple is visible. This is different from checking if
  // the ink_drop_state() == HIDDEN because the ripple may be visible while it
  // animates to the target HIDDEN state.
  virtual bool IsVisible() const = 0;

  // Sets the |center_point| of the ink drop layer relative to its parent Layer.
  virtual void SetCenterPoint(const gfx::Point& center_point) = 0;

  // Immediately aborts all in-progress animations and hides the ink drop.
  //
  // NOTE: This will NOT raise InkDropAnimation(Started|Ended) events for the
  // state transition to HIDDEN!
  virtual void HideImmediately() = 0;

 protected:
  // Notify the |observers_| that an animation has started.
  void NotifyAnimationStarted(InkDropState ink_drop_state);

  // Notify the |observers_| that an animation has ended.
  void NotifyAnimationEnded(
      InkDropState ink_drop_state,
      InkDropAnimationObserver::InkDropAnimationEndedReason reason);

 private:
  // List of observers to notify when animations have started and finished.
  base::ObserverList<InkDropAnimationObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(InkDropAnimation);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_ANIMATION_H_
