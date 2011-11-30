// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_H_
#define UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/point.h"
#include "ui/views/view.h"

namespace views {

// An interface implemented by a View that has text that can be selected.
class VIEWS_EXPORT TouchSelectionClientView
    : public View,
      public ui::SimpleMenuModel::Delegate {
 public:
  // Select everything between start and end (points are in view's local
  // coordinate system). |start| is the logical start and |end| is the logical
  // end of selection. Visually, |start| may lie after |end|.
  virtual void SelectRect(const gfx::Point& start, const gfx::Point& end) = 0;

 protected:
  virtual ~TouchSelectionClientView() {}
};

// This defines the callback interface for other code to be notified of changes
// in the state of a TouchSelectionClientView.
class VIEWS_EXPORT TouchSelectionController {
 public:
  virtual ~TouchSelectionController() {}

  // Creates a TouchSelectionController. Caller owns the returned object.
  static TouchSelectionController* create(
      TouchSelectionClientView* client_view);

  // Notification that the text selection in TouchSelectionClientView has
  // changed. p1 and p2 are lower corners of the start and end of selection:
  // ____________________________________
  // | textfield with |selected text|   |
  // ------------------------------------
  //                  ^p1           ^p2
  //
  // p1 is always the start and p2 is always the end of selection. Hence,
  // p1 could be to the right of p2 in the figure above.
  virtual void SelectionChanged(const gfx::Point& p1, const gfx::Point& p2) = 0;

  // Notification that the TouchSelectionClientView has lost focus.
  virtual void ClientViewLostFocus() = 0;
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_TOUCH_SELECTION_CONTROLLER_H_
