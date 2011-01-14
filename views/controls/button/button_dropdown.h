// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_BUTTON_DROPDOWN_H_
#define VIEWS_CONTROLS_BUTTON_BUTTON_DROPDOWN_H_
#pragma once

#include "base/task.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/menu_2.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
//
// ButtonDropDown
//
// A button class that when pressed (and held) or pressed (and drag down) will
// display a menu
//
////////////////////////////////////////////////////////////////////////////////
class ButtonDropDown : public ImageButton {
 public:
  ButtonDropDown(ButtonListener* listener, ui::MenuModel* model);
  virtual ~ButtonDropDown();

  // Accessibility accessors, overridden from View.
  virtual string16 GetAccessibleDefaultAction() OVERRIDE;
  virtual AccessibilityTypes::Role GetAccessibleRole() OVERRIDE;
  virtual AccessibilityTypes::State GetAccessibleState() OVERRIDE;

 private:
  // Overridden from CustomButton
  virtual bool OnMousePressed(const MouseEvent& e);
  virtual void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual bool OnMouseDragged(const MouseEvent& e);
  virtual void OnMouseExited(const MouseEvent& e);

  // Overridden from View. Used to display the right-click menu, as triggered
  // by the keyboard, for instance. Using the member function ShowDropDownMenu
  // for the actual display.
  virtual void ShowContextMenu(const gfx::Point& p,
                               bool is_mouse_gesture);

  // Overridden from CustomButton. Returns true if the button should become
  // pressed when a user holds the mouse down over the button. For this
  // implementation, both left and right mouse buttons can trigger a change
  // to the PUSHED state.
  virtual bool ShouldEnterPushedState(const MouseEvent& e);

  // Internal function to show the dropdown menu
  void ShowDropDownMenu(gfx::NativeView window);

  // The model that populates the attached menu.
  ui::MenuModel* model_;
  scoped_ptr<Menu2> menu_;

  // Y position of mouse when left mouse button is pressed
  int y_position_on_lbuttondown_;

  // A factory for tasks that show the dropdown context menu for the button.
  ScopedRunnableMethodFactory<ButtonDropDown> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(ButtonDropDown);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_BUTTON_DROPDOWN_H_
