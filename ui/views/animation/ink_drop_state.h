// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_STATE_H_
#define UI_VIEWS_ANIMATION_INK_DROP_STATE_H_

#include <string>

#include "ui/views/views_export.h"

namespace views {

// The different states that the ink drop animation can be animated to.
enum class InkDropState {
  // The ink drop is not visible.
  HIDDEN,
  // The view is being interacted with but the action to be triggered has not
  // yet been determined.
  ACTION_PENDING,
  // The quick action for the view has been triggered. e.g. a tap gesture to
  // click a button.
  QUICK_ACTION,
  // A view is being interacted with and the pending action will be a 'slow'
  // action. e.g. a long press that is still active before releasing.
  SLOW_ACTION_PENDING,
  // The slow action for the view has been triggered. e.g. a long press release
  // to bring up a menu.
  SLOW_ACTION,
  // An active state for a view that is not currently being interacted with.
  // e.g. a pressed button that is showing a menu.
  ACTIVATED,
  // A previously active state has been toggled to inactive. e.g. a drop down
  // menu is closed.
  DEACTIVATED,
};

// Returns a human readable string for |state|.  Useful for logging.
std::string ToString(InkDropState state);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_STATE_H_
