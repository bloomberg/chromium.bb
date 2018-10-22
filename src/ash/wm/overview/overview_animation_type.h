// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_

namespace ash {

// Enumeration of the different overview mode animations.
enum OverviewAnimationType {
  // TODO(bruthig): Remove OVERVIEW_ANIMATION_NONE value and replace it with
  // correct animation type actions.
  OVERVIEW_ANIMATION_NONE,
  // Used to fade in the close button and label.
  OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
  // Used to fade out the label.
  OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT,
  // Used to position windows when entering/exiting overview mode and when a
  // window is closed while overview mode is active.
  OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS_ON_ENTER,
  OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS_IN_OVERVIEW,
  OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS_ON_EXIT,
  // Used to restore windows to their original position when exiting overview
  // mode.
  OVERVIEW_ANIMATION_RESTORE_WINDOW,
  // Same as RESTORE_WINDOW but apply the target at the end of the animation.
  OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO,
  // Used to animate scaling down of a window that is about to get closed while
  // overview mode is active.
  OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM,
  // Used to animate hiding of a window that is closed while overview mode is
  // active.
  OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM,
  // Used to animate windows upon entering or exiting overview mode to or from
  // the home launcher.
  OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
  OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER,
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_
