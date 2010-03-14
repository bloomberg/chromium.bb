// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_ANIMATION_BOUNDS_ANIMATOR_H_
#define VIEWS_ANIMATION_BOUNDS_ANIMATOR_H_

#include <map>

#include "app/animation.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"

namespace views {

class View;

// Bounds animator is responsible for animating the bounds of a view from the
// the views current location and size to a target position and size. To use
// BoundsAnimator invoke AnimateViewTo for the set of views you want to
// animate, followed by Start to start the animation.
class BoundsAnimator : public AnimationDelegate {
 public:
  BoundsAnimator();
  ~BoundsAnimator();

  // Schedules |view| to animate from it's current bounds to |target|. If
  // |delete_when_done| is true the view is deleted when the animation
  // completes. Invoke Start to start the animation.
  void AnimateViewTo(View* view,
                     const gfx::Rect& target,
                     bool delete_when_done);

  // Returns true if BoundsAnimator is animating the bounds of |view|.
  bool IsAnimating(View* view) const;

  // Starts the animation.
  void Start();

  // Stops the animation.
  void Stop();

  // Sets the animation to use when animating changes. BoundsAnimator takes
  // ownership of |animation|. Set |is_slide| to true if |animation| is a
  // SlideAnimation.
  void SetAnimation(Animation* animation, bool is_slide);

  // AnimationDelegate overrides.
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);

 private:
  // Tracks data about the view being animated.
  struct Data {
    Data() : delete_when_done(false) {}

    // Should the view be deleted when done?
    bool delete_when_done;

    // The initial bounds.
    gfx::Rect start_bounds;

    // Target bounds.
    gfx::Rect target_bounds;
  };

  typedef std::map<View*, Data> ViewToDataMap;

  // Empties data_, deleting any views that have been marked as needing to be
  // deleted.
  void DeleteViews();

  // Mapes from view being animated to info about the view.
  ViewToDataMap data_;

  // The animation.
  scoped_ptr<Animation> animation_;

  // Is |animation_| a SlideAnimation?
  bool is_slide_;

  DISALLOW_COPY_AND_ASSIGN(BoundsAnimator);
};

}  // namespace views

#endif  // VIEWS_ANIMATION_BOUNDS_ANIMATOR_H_
