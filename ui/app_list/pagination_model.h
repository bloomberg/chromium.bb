// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_PAGINATION_MODEL_H_
#define UI_APP_LIST_PAGINATION_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_export.h"
#include "ui/base/animation/animation_delegate.h"

namespace ui {
class SlideAnimation;
}

namespace app_list {

class PaginationModelObserver;

// A simple pagination model that consists of two numbers: the total pages and
// the currently selected page. The model is a single selection model that at
// the most one page can become selected at any time.
class APP_LIST_EXPORT PaginationModel : public ui::AnimationDelegate {
 public:
  // Holds info for transition animation and touch scroll.
  struct Transition {
    Transition(int target_page, double progress)
        : target_page(target_page),
          progress(progress) {
    }

    bool operator==(const Transition& rhs) const {
      return target_page == rhs.target_page && progress == rhs.progress;
    }

    // Target page for the transition or -1 if there is no target page. For
    // page switcher, this is the target selected page. For touch scroll,
    // this is usually the previous or next page (or -1 when there is no
    // previous or next page).
    int target_page;

    // A [0, 1] progress indicates how much of the current page is being
    // transitioned.
    double progress;
  };

  PaginationModel();
  virtual ~PaginationModel();

  void SetTotalPages(int total_pages);

  // Selects a page. |animate| is true if the transition should be animated.
  void SelectPage(int page, bool animate);

  void SetTransition(const Transition& transition);
  void SetTransitionDuration(int duration_ms);

  void AddObserver(PaginationModelObserver* observer);
  void RemoveObserver(PaginationModelObserver* observer);

  int total_pages() const { return total_pages_; }
  int selected_page() const { return selected_page_; }
  const Transition& transition() const { return transition_; }

 private:
  void NotifySelectedPageChanged(int old_selected, int new_selected);
  void NotifyTransitionChanged();

  void StartTranstionAnimation(int target_page);
  void ResetTranstionAnimation();

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;

  int total_pages_;
  int selected_page_;

  Transition transition_;

  // Pending selected page when SelectedPage is called during a transition. If
  // multiple SelectPage is called while a transition is in progress, only the
  // last target page is remembered here.
  int pending_selected_page_;

  scoped_ptr<ui::SlideAnimation> transition_animation_;
  int transition_duration_ms_;  // Transition duration in millisecond.

  ObserverList<PaginationModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PaginationModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_PAGINATION_MODEL_H_
