// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ANIMATOR_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ANIMATOR_H_

#include <vector>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"

// Animates the tabs in the tabstrip. Must be kept abreast of non-
// animated changes to tabs as well via the *NoAnimation methods.
class TabStripAnimator {
 public:
  explicit TabStripAnimator(base::RepeatingClosure on_animation_progressed);
  ~TabStripAnimator();

  std::vector<TabAnimationState> GetCurrentTabStates() const;
  bool IsAnimating() const;

  // Inserts, without animation, a new tab at |index|.
  // |tab_removed_callback| will be invoked if the tab is removed
  // at the end of a remove animation.
  void InsertTabAtNoAnimation(int index,
                              base::OnceClosure tab_removed_callback,
                              TabAnimationState::TabActiveness active,
                              TabAnimationState::TabPinnedness pinned);

  // Removes the tab at |index| from the strip without playing any animation.
  // Does not invoke the associated |tab_removed_callback|.
  void RemoveTabNoAnimation(int index);

  // TODO(958173): Temporary methods to keep the animator's state in sync with
  // changes it is not responsible for animating.
  void MoveTabNoAnimation(int prev_index, int new_index);
  void SetPinnednessNoAnimation(int index,
                                TabAnimationState::TabPinnedness pinnedness);

  // Animates the insertion of a new tab at |index|.
  // |tab_removed_callback| will be invoked if the tab is removed
  // at the end of a remove animation.
  void InsertTabAt(int index,
                   base::OnceClosure tab_removed_callback,
                   TabAnimationState::TabActiveness active,
                   TabAnimationState::TabPinnedness pinned);

  // Animates the tab being removed from the strip. That animation will
  // invoke the associated |tab_removed_callback| when complete.
  void RemoveTab(int index);

  void SetActiveTab(int prev_active_index, int new_active_index);

  // TODO(958173): Implement move and pin animations.

  void CompleteAnimations();

  // TODO(958173): Temporary method that aborts current animations, leaving
  // tabs where they are. Use to hand off animation responsibilities from
  // this animator to elsewhere without teleporting tabs.
  void CancelAnimations();

 private:
  void AnimateTabTo(int index, TabAnimationState target_state);
  void TickAnimations();
  void RemoveClosedTabs();

  base::RepeatingClosure on_animation_progressed_callback_;
  std::vector<TabAnimation> animations_;

  base::RepeatingTimer timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_ANIMATOR_H_
