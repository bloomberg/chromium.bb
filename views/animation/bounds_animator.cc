// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/animation/bounds_animator.h"

#include "app/slide_animation.h"
#include "base/compiler_specific.h"
#include "views/view.h"

namespace views {

BoundsAnimator::BoundsAnimator()
    : ALLOW_THIS_IN_INITIALIZER_LIST(animation_(new SlideAnimation(this))),
      is_slide_(true) {
}

BoundsAnimator::~BoundsAnimator() {
  data_.clear();
  animation_->set_delegate(NULL);
  animation_.reset();
}

void BoundsAnimator::AnimateViewTo(View* view,
                                   const gfx::Rect& target,
                                   bool delete_when_done) {
  Data& data = data_[view];
  data.start_bounds = view->bounds();
  data.target_bounds = target;
  data.delete_when_done = delete_when_done;
}

bool BoundsAnimator::IsAnimating(View* view) const {
  return data_.find(view) != data_.end();
}

void BoundsAnimator::Start() {
  // Unset the delegate so that we don't attempt to cleanup if the animation is
  // running and we cancel it.
  animation_->set_delegate(NULL);

  animation_->Stop();

  if (is_slide_) {
    // TODO(sky): this is yucky, need a better way to express this.
    static_cast<SlideAnimation*>(animation_.get())->Hide();
    static_cast<SlideAnimation*>(animation_.get())->Reset();
    static_cast<SlideAnimation*>(animation_.get())->Show();
  } else {
    animation_->Start();
  }

  animation_->set_delegate(this);
}

void BoundsAnimator::Stop() {
  animation_->set_delegate(NULL);
  animation_->Stop();
  animation_->set_delegate(this);

  DeleteViews();
  data_.clear();
}

void BoundsAnimator::SetAnimation(Animation* animation, bool is_slide) {
  animation_.reset(animation);
  is_slide_ = is_slide;
}

void BoundsAnimator::AnimationProgressed(const Animation* animation) {
  gfx::Rect repaint_bounds;

  for (ViewToDataMap::const_iterator i = data_.begin(); i != data_.end(); ++i) {
    View* view = i->first;
    gfx::Rect new_bounds =
        animation_->CurrentValueBetween(i->second.start_bounds,
                                        i->second.target_bounds);
    if (new_bounds != view->bounds()) {
      gfx::Rect total_bounds = new_bounds.Union(view->bounds());
      if (repaint_bounds.IsEmpty())
        repaint_bounds = total_bounds;
      else
        repaint_bounds = repaint_bounds.Union(total_bounds);
      view->SetBounds(new_bounds);
    }
  }

  if (!data_.empty() && !repaint_bounds.IsEmpty())
    data_.begin()->first->GetParent()->SchedulePaint(repaint_bounds, false);
}

void BoundsAnimator::AnimationEnded(const Animation* animation) {
  DeleteViews();
}

void BoundsAnimator::AnimationCanceled(const Animation* animation) {
}

void BoundsAnimator::DeleteViews() {
  for (ViewToDataMap::iterator i = data_.begin(); i != data_.end(); ++i) {
    if (i->second.delete_when_done) {
      View* view = i->first;
      view->GetParent()->RemoveChildView(view);
      delete view;
    }
  }
  data_.clear();
}

}  // namespace views
