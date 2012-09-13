// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/pagination_model.h"

#include <algorithm>

#include "ui/app_list/pagination_model_observer.h"
#include "ui/base/animation/slide_animation.h"

namespace app_list {

PaginationModel::PaginationModel()
    : total_pages_(-1),
      selected_page_(-1),
      transition_(-1, 0),
      scrolling_(false),
      pending_selected_page_(-1),
      transition_duration_ms_(0) {
}

PaginationModel::~PaginationModel() {
}

void PaginationModel::SetTotalPages(int total_pages) {
  if (total_pages == total_pages_)
    return;

  total_pages_ = total_pages;
  if (selected_page_ < 0)
    SelectPage(0, false /* animate */);
  if (selected_page_ >= total_pages_)
    SelectPage(total_pages_ - 1, false /* animate */);
  FOR_EACH_OBSERVER(PaginationModelObserver, observers_, TotalPagesChanged());
}

void PaginationModel::SelectPage(int page, bool animate) {
  if (animate) {
    // -1 and |total_pages_| are valid target page for animation.
    DCHECK(page >= -1 && page <= total_pages_);

    if (!transition_animation_.get()) {
      if (page == selected_page_)
        return;

      // Creates an animation if there is not one.
      StartTranstionAnimation(page);
      return;
    } else {
      const bool showing = transition_animation_->IsShowing();
      const int from_page = showing ? selected_page_ :  transition_.target_page;
      const int to_page = showing ? transition_.target_page : selected_page_;

      if (from_page == page) {
        if (showing)
          transition_animation_->Hide();
        else
          transition_animation_->Show();
        pending_selected_page_ = -1;
      } else if (to_page != page) {
        pending_selected_page_ = page;
      } else {
        pending_selected_page_ = -1;
      }
    }
  } else {
    DCHECK(page >= 0 && page < total_pages_);

    if (page == selected_page_)
      return;

    ResetTransitionAnimation();

    int old_selected = selected_page_;
    selected_page_ = page;
    NotifySelectedPageChanged(old_selected, selected_page_);
  }
}

void PaginationModel::SelectPageRelative(int delta, bool animate) {
  SelectPage(CalculateTargetPage(delta), animate);
}

void PaginationModel::SetTransition(const Transition& transition) {
  // -1 and |total_pages_| is a valid target page, which means user is at
  // the end and there is no target page for this scroll.
  DCHECK(transition.target_page >= -1 &&
         transition.target_page <= total_pages_);
  DCHECK(transition.progress >= 0 && transition.progress <= 1);

  if (transition_.Equals(transition))
    return;

  transition_ = transition;
  NotifyTransitionChanged();
}

void PaginationModel::SetTransitionDuration(int duration_ms) {
  transition_duration_ms_ = duration_ms;
}

void PaginationModel::StartScroll() {
  scrolling_ = true;
  // Cancels current transition animation (if any).
  transition_animation_.reset();
}

void PaginationModel::UpdateScroll(double delta) {
  // Translates scroll delta to desired page change direction.
  int page_change_dir = delta > 0 ? -1 : 1;

  // Initializes a transition if there is none.
  if (!has_transition())
    transition_.target_page = CalculateTargetPage(page_change_dir);

  // Updates transition progress.
  int transition_dir = transition_.target_page > selected_page_ ? 1 : -1;
  double progress = transition_.progress +
      fabs(delta) * page_change_dir * transition_dir;

  if (progress < 0) {
    clear_transition();
  } else if (progress > 1) {
    if (is_valid_page(transition_.target_page)) {
      SelectPage(transition_.target_page, false);
      clear_transition();
    }
  } else {
    transition_.progress = progress;
    NotifyTransitionChanged();
  }
}

void PaginationModel::EndScroll(bool cancel) {
  scrolling_ = false;

  if (!has_transition())
    return;

  CreateTransitionAnimation();
  transition_animation_->Reset(transition_.progress);

  // Always call Show to ensure animation will run.
  transition_animation_->Show();
  if (cancel)
    transition_animation_->Hide();
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

int PaginationModel::CalculateTargetPage(int delta) const {
  DCHECK_GT(total_pages_, 0);

  int current_page = selected_page_;
  if (transition_animation_.get() && transition_animation_->IsShowing()) {
    current_page = pending_selected_page_ >= 0 ?
        pending_selected_page_ : transition_.target_page;
  }

  const int target_page = current_page + delta;

  int start_page = 0;
  int end_page = total_pages_ - 1;

  // Use invalid page when |selected_page_| is at ends.
  if (target_page < start_page && selected_page_ == start_page)
    start_page = -1;
  else if (target_page > end_page && selected_page_ == end_page)
    end_page = total_pages_;

  return std::max(start_page, std::min(end_page, target_page));
}

void PaginationModel::StartTranstionAnimation(int target_page) {
  DCHECK(selected_page_ != target_page);

  SetTransition(Transition(target_page, 0));
  CreateTransitionAnimation();
  transition_animation_->Show();
}

void PaginationModel::CreateTransitionAnimation() {
  transition_animation_.reset(new ui::SlideAnimation(this));
  transition_animation_->SetTweenType(ui::Tween::LINEAR);
  if (transition_duration_ms_)
    transition_animation_->SetSlideDuration(transition_duration_ms_);
}

void PaginationModel::ResetTransitionAnimation() {
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

  if (transition_animation_->GetCurrentValue() == 1) {
    // Showing animation ends.
    int target_page = transition_.target_page;

    // If target page is not in valid range, reverse the animation.
    if (target_page < 0 || target_page >= total_pages_) {
      transition_animation_->Hide();
      return;
    }

    // Otherwise, change page and finish the transition.
    DCHECK(selected_page_ != transition_.target_page);
    SelectPage(transition_.target_page, false /* animate */);
  } else if (transition_animation_->GetCurrentValue() == 0) {
    // Hiding animation ends. No page change should happen.
    ResetTransitionAnimation();
  }

  if (next_target >= 0)
    SelectPage(next_target, true);
}

}  // namespace app_list
