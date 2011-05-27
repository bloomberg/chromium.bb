// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#define VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
#pragma once

#include <string>

#include "views/controls/button/native_button.h"
#include "views/controls/button/text_button.h"

namespace views {

class Label;

// A NativeButton subclass representing a checkbox.
class Checkbox : public NativeButtonBase {
 public:
  // The button's class name.
  static const char kViewClassName[];

  Checkbox();
  explicit Checkbox(const std::wstring& label);
  virtual ~Checkbox();

  // Returns the indentation of the text from the left edge of the view.
  static int GetTextIndent();

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

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int w) OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseMoved(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from NativeButton:
  virtual void SetLabel(const std::wstring& label) OVERRIDE;

 protected:
  // Returns true if the event (in Checkbox coordinates) is within the bounds of
  // the label.
  bool HitTestLabel(const MouseEvent& event);

  // Overridden from View:
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // Overridden from NativeButton:
  virtual NativeButtonWrapper* CreateWrapper() OVERRIDE;
  virtual void InitBorder() OVERRIDE;

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

// A native themed class representing a checkbox.  This class does not use
// platform specific objects to replicate the native platforms looks and feel.
//
// This class will eventually be renamed to Checkbox to replace the class
// above.
class CheckboxNt : public TextButtonBase {
 public:
  explicit CheckboxNt(const std::wstring& label);
  virtual ~CheckboxNt();

  // Sets a listener for this checkbox. Checkboxes aren't required to have them
  // since their state can be read independently of them being toggled.
  void set_listener(ButtonListener* listener) { listener_ = listener; }

  // Sets/Gets whether or not the checkbox is checked.
  virtual void SetChecked(bool checked);
  bool checked() const { return checked_; }

 protected:
  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

 private:
  // Overridden from Button:
  virtual void NotifyClick(const views::Event& event) OVERRIDE;

  // Overridden from TextButtonBase:
  virtual gfx::NativeTheme::Part GetThemePart() const OVERRIDE;
  virtual gfx::Rect GetThemePaintRect() const OVERRIDE;
  virtual void GetExtraParams(
      gfx::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual gfx::Rect GetTextBounds() const OVERRIDE;

  // True if the checkbox is checked.
  bool checked_;

  // The button's class name.
  static const char kViewClassName[];

  DISALLOW_COPY_AND_ASSIGN(CheckboxNt);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_BUTTON_CHECKBOX_H_
