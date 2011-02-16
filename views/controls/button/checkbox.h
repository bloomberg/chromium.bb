// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#define VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#pragma once

#include <string>

#include "views/controls/button/native_button.h"

namespace views {

class Label;

// A NativeButton subclass representing a checkbox.
class Checkbox : public NativeButton {
 public:
  // The button's class name.
  static const char kViewClassName[];

  Checkbox();
  explicit Checkbox(const std::wstring& label);
  virtual ~Checkbox();

  // Sets a listener for this checkbox. Checkboxes aren't required to have them
  // since their state can be read independently of them being toggled.
  void set_listener(ButtonListener* listener) { listener_ = listener; }

  // Sets whether or not the checkbox label should wrap multiple lines of text.
  // If true, long lines are wrapped, and this is reflected in the preferred
  // size returned by GetPreferredSize. If false, text that will not fit within
  // the available bounds for the label will be cropped.
  void SetMultiLine(bool multiline);

  // Sets/Gets whether or not the checkbox is checked.
  virtual void SetChecked(bool checked);
  bool checked() const { return checked_; }

  // Returns the indentation of the text from the left edge of the view.
  static int GetTextIndent();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize();
  virtual int GetHeightForWidth(int w);
  virtual void Layout();
  virtual void SetEnabled(bool enabled);
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas);
  virtual View* GetViewForPoint(const gfx::Point& point);
  virtual View* GetViewForPoint(const gfx::Point& point,
                                bool can_create_floating);
  virtual void OnMouseEntered(const MouseEvent& e);
  virtual void OnMouseMoved(const MouseEvent& e);
  virtual void OnMouseExited(const MouseEvent& e);
  virtual bool OnMousePressed(const MouseEvent& e);
  virtual void OnMouseReleased(const MouseEvent& e, bool canceled);
  virtual bool OnMouseDragged(const MouseEvent& e);
  virtual void WillGainFocus();
  virtual void WillLoseFocus();

  // Accessibility accessors, overridden from View.
  virtual AccessibilityTypes::Role GetAccessibleRole();
  virtual AccessibilityTypes::State GetAccessibleState();

  // Overridden from NativeButton:
  virtual void SetLabel(const std::wstring& label);

 protected:
  virtual std::string GetClassName() const;

  // Overridden from NativeButton:
  virtual NativeButtonWrapper* CreateWrapper();
  virtual void InitBorder();

  // Returns true if the event (in Checkbox coordinates) is within the bounds of
  // the label.
  bool HitTestLabel(const MouseEvent& e);

 private:
  // Called from the constructor to create and configure the checkbox label.
  void Init(const std::wstring& label_text);

  // The checkbox's label. We may not be able to use the OS version on some
  // platforms because of transparency and sizing issues.
  Label* label_;

  // True if the checkbox is checked.
  bool checked_;

  DISALLOW_COPY_AND_ASSIGN(Checkbox);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
