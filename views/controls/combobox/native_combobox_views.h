// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_VIEWS_H_
#define VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_VIEWS_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"
#include "views/controls/combobox/native_combobox_wrapper.h"
#include "views/view.h"

namespace gfx {
class Canvas;
class Font;
}

namespace views {

class KeyEvent;
class Menu2;
class FocusableBorder;

// A views/skia only implementation of NativeComboboxWrapper.
// No platform specific code is used.
class NativeComboboxViews : public views::View,
                            public NativeComboboxWrapper,
                            public ui::SimpleMenuModel::Delegate {
 public:
  explicit NativeComboboxViews(Combobox* parent);
  virtual ~NativeComboboxViews();

  // views::View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& event) OVERRIDE;
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
  virtual gfx::NativeView GetTestingHandle() const OVERRIDE;

  // ui::SimpleMenuModel::Delegate overrides
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // class name of internal
  static const char kViewClassName[];

  // Returns true when
  // 1) built with GYP_DEFINES="touchui=1"
  // 2) enabled by SetEnableComboboxViews(true)
  // 3) enabled by the command line flag "--enable-combobox-view")
  static bool IsComboboxViewsEnabled();
  // Enable/Disable NativeComboboxViews implementation for Combobox.
  static void SetEnableComboboxViews(bool enabled);

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

  // Context menu and its content list for the combobox.
  scoped_ptr<ui::SimpleMenuModel> dropdown_list_model_;
  scoped_ptr<Menu2> dropdown_list_menu_;

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

#endif  // VIEWS_CONTROLS_COMBOBOX_NATIVE_COMBOBOX_VIEWS_H_
