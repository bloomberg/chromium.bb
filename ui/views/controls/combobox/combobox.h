// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#pragma once

#include <string>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/combobox/native_combobox_wrapper.h"
#include "views/view.h"

namespace ui {
class ComboboxModel;
}

namespace views {

class ComboboxListener;

// A non-editable combo-box (aka a drop-down list)
class VIEWS_EXPORT Combobox : public View {
 public:
  // The combobox's class name.
  static const char kViewClassName[];

  // |model| is not owned by the combo box.
  explicit Combobox(ui::ComboboxModel* model);
  virtual ~Combobox();

  // Register |listener| for item change events.
  void set_listener(ComboboxListener* listener) {
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
  ui::ComboboxModel* model() const { return model_; }

  // Set the accessible name of the combo box.
  void SetAccessibleName(const string16& name);

  // Provided only for testing:
  gfx::NativeView GetTestingHandle() const {
    return native_wrapper_ ? native_wrapper_->GetTestingHandle() : NULL;
  }
  NativeComboboxWrapper* GetNativeWrapperForTesting() const {
    return native_wrapper_;
  }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnEnabledChanged() OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e) OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnKeyPressed(const views::KeyEvent& e) OVERRIDE;
  virtual bool OnKeyReleased(const views::KeyEvent& e) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // The object that actually implements the native combobox.
  NativeComboboxWrapper* native_wrapper_;

 private:
  // Our model.
  ui::ComboboxModel* model_;

  // The combobox's listener. Notified when the selected item change.
  ComboboxListener* listener_;

  // The current selection.
  int selected_item_;

  // The accessible name of the text field.
  string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(Combobox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
