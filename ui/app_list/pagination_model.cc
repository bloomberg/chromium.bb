// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/pagination_model.h"

#include "ui/app_list/pagination_model_observer.h"
#include "ui/base/animation/slide_animation.h"

namespace app_list {

PaginationModel::PaginationModel()
    : total_pages_(-1),
      selected_page_(-1),
      transition_(-1, 0),
      pending_selected_page_(-1),
      transition_duration_ms_(0) {
}

PaginationModel::~PaginationModel() {
}

void PaginationModel::SetTotalPages(int total_pages) {
  if (total_pages == total_pages_)
    return;

  total_pages_ = total_pages;
  FOR_EACH_OBSERVER(PaginationModelObserver,
                    observers_,
                    TotalPagesChanged());
}

void PaginationModel::SelectPage(int page, bool animate) {
  DCHECK(page >= 0 && page < total_pages_);

  if (animate) {
    if (!transition_animation_.get()) {
      if (page == selected_page_)
        return;

      // Creates an animation if there is not one.
      StartTranstionAnimation(page);
      return;
    } else if (transition_.target_page != page) {
      // If there is a running transition, remembers it if it's new.
      pending_selected_page_ = page;
    }
  } else {
    if (page == selected_page_)
      return;

    ResetTranstionAnimation();

    int old_selected = selected_page_;
    selected_page_ = page;
    NotifySelectedPageChanged(old_selected, selected_page_);
  }
}

void PaginationModel::SetTransition(const Transition& transition) {
  // -1 is a valid target page, which means user is at the end and there is no
  // target page for this scroll.
  DCHECK(transition.target_page >= -1 && transition.target_page < total_pages_);
  DCHECK(transition.progress >= 0 && transition.progress <= 1);

  if (transition_ == transition)
    return;

  transition_ = transition;
  NotifyTransitionChanged();
}

void PaginationModel::SetTransitionDuration(int duration_ms) {
  transition_duration_ms_ = duration_ms;
}

void PaginationModel::AddObserver(PaginationModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PaginationModel::RemoveObserver(PaginationModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PaginationModel::NotifySelectedPageChanged(int old_selected,
                                                int new_selected) {
  FOR_EACH_OBSERVER(PaginationModelObserver,
                    observers_,
                    SelectedPageChanged(old_selected, new_selected));
}

void PaginationModel::NotifyTransitionChanged() {
  FOR_EACH_OBSERVER(PaginationModelObserver, observers_, TransitionChanged());
}

void PaginationModel::StartTranstionAnimation(int target_page) {
  SetTransition(Transition(target_page, 0));
  transition_animation_.reset(new ui::SlideAnimation(this));
  if (transition_duration_ms_)
    transition_animation_->SetSlideDuration(transition_duration_ms_);
  transition_animation_->Show();
}

void PaginationModel::ResetTranstionAnimation() {
  transition_animation_.reset();
  transition_.target_page = -1;
  transition_.progress = 0;
  pending_selected_page_ = -1;
}

void PaginationModel::AnimationProgressed(const ui::Animation* animation) {
  transition_.progress = transition_animation_->GetCurrentValue();
  NotifyTransitionChanged();
}

void PaginationModel::AnimationEnded(const ui::Animation* animation) {
  // Save |pending_selected_page_| because SelectPage resets it.
  int next_target = pending_selected_page_;
  SelectPage(transition_.target_page, false /* animte */);
  if (next_target >= 0)
    StartTranstionAnimation(next_target);
}

}  // namespace app_list
