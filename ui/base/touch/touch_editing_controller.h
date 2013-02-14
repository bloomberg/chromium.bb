// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TOUCH_TOUCH_EDITING_CONTROLLER_H_
#define UI_BASE_TOUCH_TOUCH_EDITING_CONTROLLER_H_

#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace ui {

// An interface implemented by widget that has text that can be selected/edited
// using touch.
class UI_EXPORT TouchEditable : public ui::SimpleMenuModel::Delegate {
 public:
  // Select everything between start and end (points are in view's local
  // coordinate system). |start| is the logical start and |end| is the logical
  // end of selection. Visually, |start| may lie after |end|.
  virtual void SelectRect(const gfx::Point& start, const gfx::Point& end) = 0;

  // Gets the bounds of the client view in parent's coordinates.
  virtual const gfx::Rect& GetBounds() = 0;

  // Gets the NativeView hosting the client.
  virtual gfx::NativeView GetNativeView() = 0;

  // Converts a point to/from screen coordinates from/to client view.
  virtual void ConvertPointToScreen(gfx::Point* point) = 0;
  virtual void ConvertPointFromScreen(gfx::Point* point) = 0;

 protected:
  virtual ~TouchEditable() {}
};

// This defines the callback interface for other code to be notified of changes
// in the state of a TouchEditable.
class UI_EXPORT TouchSelectionController {
 public:
  virtual ~TouchSelectionController() {}

  // Creates a TouchSelectionController. Caller owns the returned object.
  static TouchSelectionController* create(
      TouchEditable* client_view);

  // Notification that the text selection in TouchEditable has
  // changed. p1 and p2 are lower corners of the start and end of selection:
  // ____________________________________
  // | textfield with |selected text|   |
  // ------------------------------------
  //                  ^p1           ^p2
  //
  // p1 is always the start and p2 is always the end of selection. Hence,
  // p1 could be to the right of p2 in the figure above.
  virtual void SelectionChanged(const gfx::Point& p1, const gfx::Point& p2) = 0;

  // Notification that the TouchEditable has lost focus.
  virtual void ClientViewLostFocus() = 0;
};

}  // namespace views

#endif  // UI_BASE_TOUCH_TOUCH_EDITING_CONTROLLER_H_
