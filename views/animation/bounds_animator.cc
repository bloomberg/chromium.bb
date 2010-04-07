// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/animation/bounds_animator.h"

#include "app/slide_animation.h"
#include "base/scoped_ptr.h"
#include "views/view.h"

// Duration in milliseconds for animations.
static const int kAnimationDuration = 200;

namespace views {

BoundsAnimator::BoundsAnimator(View* parent)
    : parent_(parent),
      observer_(NULL),
      container_(new AnimationContainer()) {
  container_->set_observer(this);
}

BoundsAnimator::~BoundsAnimator() {
  // Reset the delegate so that we don't attempt to notify our observer from
  // the destructor.
  container_->set_observer(NULL);

  // Delete all the animations, but don't remove any child views. We assume the
  // view owns us and is going to be deleted anyway.
  for (ViewToDataMap::iterator i = data_.begin(); i != data_.end(); ++i)
    CleanupData(false, &(i->second), i->first);
}

void BoundsAnimator::AnimateViewTo(View* view,
                                   const gfx::Rect& target,
                                   bool delete_when_done) {
  DCHECK_EQ(view->GetParent(), parent_);

  Data existing_data;

  if (data_.count(view) > 0) {
    // Don't immediatly delete the animation, that might trigger a callback from
    // the animationcontainer.
    existing_data = data_[view];

    RemoveFromMaps(view);
  }

  // NOTE: we don't check if the view is already at the target location. Doing
  // so leads to odd cases where no animations may be present after invoking
  // AnimateViewTo. AnimationProgressed does nothing when the bounds of the
  // view don't change.

  Data& data = data_[view];
  data.start_bounds = view->bounds();
  data.target_bounds = target;
  data.animation = CreateAnimation();
  data.delete_when_done = delete_when_done;

  animation_to_view_[data.animation] = view;

  data.animation->Show();

  CleanupData(true, &existing_data, NULL);
}

void BoundsAnimator::SetAnimationForView(View* view,
                                         SlideAnimation* animation) {
  scoped_ptr<SlideAnimation> animation_wrapper(animation);
  if (data_.find(view) == data_.end())
    return;

  // We delay deleting the animation until the end so that we don't prematurely
  // send out notification that we're done.
  scoped_ptr<Animation> old_animation(ResetAnimationForView(view));

  data_[view].animation = animation_wrapper.release();
  animation_to_view_[animation] = view;

  animation->set_delegate(this);
  animation->SetContainer(container_.get());
  animation->Show();
}

const SlideAnimation* BoundsAnimator::GetAnimationForView(View* view) {
  return data_.find(view) == data_.end() ? NULL : data_[view].animation;
}

void BoundsAnimator::SetAnimationDelegate(View* view,
                                          AnimationDelegate* delegate,
                                          bool delete_when_done) {
  DCHECK(IsAnimating(view));
  data_[view].delegate = delegate;
  data_[view].delete_delegate_when_done = delete_when_done;
}

void BoundsAnimator::StopAnimatingView(View* view) {
  if (data_.find(view) == data_.end())
    return;

  data_[view].animation->Stop();
}

bool BoundsAnimator::IsAnimating(View* view) const {
  return data_.find(view) != data_.end();
}

bool BoundsAnimator::IsAnimating() const {
  return !data_.empty();
}

void BoundsAnimator::Cancel() {
  if (data_.empty())
    return;

  while (!data_.empty())
    data_.begin()->second.animation->Stop();

  // Invoke AnimationContainerProgressed to force a repaint and notify delegate.
  AnimationContainerProgressed(container_.get());
}

SlideAnimation* BoundsAnimator::CreateAnimation() {
  SlideAnimation* animation = new SlideAnimation(this);
  animation->SetContainer(container_.get());
  animation->SetSlideDuration(kAnimationDuration);
  animation->SetTweenType(SlideAnimation::EASE_OUT);
  return animation;
}

void BoundsAnimator::RemoveFromMaps(View* view) {
  DCHECK(data_.count(view) > 0);

  animation_to_view_.erase(data_[view].animation);
  data_.erase(view);
}

void BoundsAnimator::CleanupData(bool send_cancel, Data* data, View* view) {
  if (send_cancel && data->delegate)
    data->delegate->AnimationCanceled(data->animation);

  if (data->delete_delegate_when_done) {
    delete static_cast<OwnedAnimationDelegate*>(data->delegate);
    data->delegate = NULL;
  }

  if (data->delete_when_done)
    delete view;

  delete data->animation;
  data->animation = NULL;
}

Animation* BoundsAnimator::ResetAnimationForView(View* view) {
  if (data_.find(view) == data_.end())
    return NULL;

  Animation* old_animation = data_[view].animation;
  animation_to_view_.erase(old_animation);
  data_[view].animation = NULL;
  // Reset the delegate so that we don't attempt any processing when the
  // animation calls us back.
  old_animation->set_delegate(NULL);
  return old_animation;
}

void BoundsAnimator::AnimationProgressed(const Animation* animation) {
  DCHECK(animation_to_view_.find(animation) != animation_to_view_.end());

  View* view = animation_to_view_[animation];
  const Data& data = data_[view];
  gfx::Rect new_bounds =
      animation->CurrentValueBetween(data.start_bounds, data.target_bounds);
  if (new_bounds != view->bounds()) {
    gfx::Rect total_bounds = new_bounds.Union(view->bounds());

    // Build up the region to repaint in repaint_bounds_. We'll do the repaint
    // when all animations complete (in AnimationContainerProgressed).
    if (repaint_bounds_.IsEmpty())
      repaint_bounds_ = total_bounds;
    else
      repaint_bounds_ = repaint_bounds_.Union(total_bounds);

    view->SetBounds(new_bounds);
  }

  if (data_[view].delegate)
    data_[view].delegate->AnimationProgressed(animation);
}

void BoundsAnimator::AnimationEnded(const Animation* animation) {
  View* view = animation_to_view_[animation];
  AnimationDelegate* delegate = data_[view].delegate;

  // Make a copy of the data as Remove empties out the maps.
  Data data = data_[view];

  RemoveFromMaps(view);

  if (delegate)
    delegate->AnimationEnded(animation);

  CleanupData(false, &data, view);
}

void BoundsAnimator::AnimationCanceled(const Animation* animation) {
  View* view = animation_to_view_[animation];
  AnimationDelegate* delegate = data_[view].delegate;

  // Make a copy of the data as Remove empties out the maps.
  Data data = data_[view];

  RemoveFromMaps(view);

  if (delegate)
    delegate->AnimationCanceled(animation);

  CleanupData(false, &data, view);
}

void BoundsAnimator::AnimationContainerProgressed(
    AnimationContainer* container) {
  if (!repaint_bounds_.IsEmpty()) {
    parent_->SchedulePaint(repaint_bounds_, false);
    repaint_bounds_.SetRect(0, 0, 0, 0);
  }

  if (observer_ && !IsAnimating()) {
    // Notify here rather than from AnimationXXX to avoid deleting the animation
    // while the animaion is calling us.
    observer_->OnBoundsAnimatorDone(this);
  }
}

void BoundsAnimator::AnimationContainerEmpty(AnimationContainer* container) {
}

}  // namespace views
