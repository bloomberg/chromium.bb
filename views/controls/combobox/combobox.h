// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#define VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#pragma once

#include <string>

#include "views/view.h"

#ifdef UNIT_TEST
#include "ui/gfx/native_widget_types.h"
#include "views/controls/combobox/native_combobox_wrapper.h"
#endif

namespace ui {
class ComboboxModel;
}
using ui::ComboboxModel;

namespace views {

class NativeComboboxWrapper;

// A non-editable combo-box (aka a drop-down list)
class Combobox : public View {
 public:
  // The combobox's class name.
  static const char kViewClassName[];

  class Listener {
   public:
    // This is invoked once the selected item changed.
    virtual void ItemChanged(Combobox* combo_box,
                             int prev_index,
                             int new_index) = 0;

   protected:
    virtual ~Listener() {}
  };

  // |model| is not owned by the combo box.
  explicit Combobox(ComboboxModel* model);
  virtual ~Combobox();

  // Register |listener| for item change events.
  void set_listener(Listener* listener) {
    listener_ = listener;
  }

  // Inform the combo box that its model changed.
  void ModelChanged();

  // Gets/Sets the selected item.
  int selected_item() const { return selected_item_; }
  void SetSelectedItem(int index);

  // Called when the combo box's selection is changed by the user.
  void SelectionChanged();

  // Accessor for |model_|.
  ComboboxModel* model() const { return model_; }

  // Set the accessible name of the combo box.
  void SetAccessibleName(const string16& name);

#ifdef UNIT_TEST
  gfx::NativeView GetTestingHandle() const {
    return native_wrapper_ ? native_wrapper_->GetTestingHandle() : NULL;
  }
  NativeComboboxWrapper* native_wrapper() const {
    return native_wrapper_;
  }
#endif

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void SetEnabled(bool enabled) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& e) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& e) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // The object that actually implements the native combobox.
  NativeComboboxWrapper* native_wrapper_;

 private:
  // Our model.
  ComboboxModel* model_;

  // Item change listener.
  Listener* listener_;

  // The current selection.
  int selected_item_;

  // The accessible name of the text field.
  string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(Combobox);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
