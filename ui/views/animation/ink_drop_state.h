// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_STATE_H_
#define UI_VIEWS_ANIMATION_INK_DROP_STATE_H_

namespace views {

// The different states that the ink drop animation can be animated to.
enum class InkDropState {
  // The ink drop is not visible.
  HIDDEN,
  // The view is being interacted with but the action to be triggered has not
  // yet been determined.
  ACTION_PENDING,
  // The quick action for the view has been triggered. e.g. A tap gesture to
  // click a button.
  QUICK_ACTION,
  // The slow action for the view has been triggered. e.g. A long press to bring
  // up a menu.
  SLOW_ACTION,
  // An active state for a view that is no longer being interacted with. e.g. A
  // pressed button that is showing a menu.
  ACTIVATED
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_STATE_H_
