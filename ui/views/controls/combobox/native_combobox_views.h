// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_VIEWS_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_VIEWS_H_
#pragma once

#include "ui/views/controls/combobox/native_combobox_wrapper.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
class Font;
}

namespace views {

class KeyEvent;
class FocusableBorder;
class MenuRunner;

// A views/skia only implementation of NativeComboboxWrapper.
// No platform specific code is used.
class NativeComboboxViews : public views::View,
                            public NativeComboboxWrapper,
                            public views::MenuDelegate {
 public:
  explicit NativeComboboxViews(Combobox* parent);
  virtual ~NativeComboboxViews();

  // views::View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& mouse_event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& mouse_event) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& key_event) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& key_event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // NativeComboboxWrapper overrides:
  virtual void UpdateFromModel() OVERRIDE;
  virtual void UpdateSelectedItem() OVERRIDE;
  virtual void UpdateEnabled() OVERRIDE;
  virtual int GetSelectedItem() const OVERRIDE;
  virtual bool IsDropdownOpen() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual View* GetView() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual bool HandleKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool HandleKeyReleased(const views::KeyEvent& event) OVERRIDE;
  virtual void HandleFocus() OVERRIDE;
  virtual void HandleBlur() OVERRIDE;
  virtual gfx::NativeView GetTestingHandle() const OVERRIDE;

  // MenuDelegate overrides:
  virtual bool IsItemChecked(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual bool GetAccelerator(int id, ui::Accelerator* accelerator) OVERRIDE;

  // class name of internal
  static const char kViewClassName[];

 private:
  // Returns the Combobox's font.
  const gfx::Font& GetFont() const;

  // Draw an arrow
  void DrawArrow(gfx::Canvas* canvas,
                 int tip_x, int tip_y, int shift_x, int shift_y) const;

  // Draw the selected value of the drop down list
  void PaintText(gfx::Canvas* canvas);

  // Show the drop down list
  void ShowDropDownMenu();

  // The parent combobox, the owner of this object.
  Combobox* combobox_;

  // The reference to the border class. The object is owned by View::border_.
  FocusableBorder* text_border_;

  // Responsible for showing the context menu.
  scoped_ptr<MenuRunner> dropdown_list_menu_runner_;

  // Is the drop down list showing
  bool dropdown_open_;

  // Index in the model of the selected item: -1 => none
  int selected_item_;

  // The maximum dimensions of the content in the dropdown
  int content_width_;
  int content_height_;

  DISALLOW_COPY_AND_ASSIGN(NativeComboboxViews);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_VIEWS_H_
