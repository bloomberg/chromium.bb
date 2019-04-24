// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace ui {
class ComboboxModel;
class NativeTheme;
}  // namespace ui

namespace views {
class EditableComboboxMenuModel;
class EditableComboboxListener;
class MenuRunner;
class Textfield;

// Textfield that also shows a drop-down list with suggestions.
class VIEWS_EXPORT EditableCombobox : public View,
                                      public TextfieldController,
                                      public ViewObserver {
 public:
  // The class name.
  static const char kViewClassName[];
  static const int kDefaultTextContext = style::CONTEXT_BUTTON;
  static const int kDefaultTextStyle = style::STYLE_PRIMARY;

  // |combobox_model|: The ComboboxModel that gives us the items to show in the
  // menu.
  // |filter_on_edit|: Whether to only show the items that are case-insensitive
  // completions of the current textfield content.
  // |show_on_empty|: Whether to show the drop-down list when there is no
  // textfield content.
  explicit EditableCombobox(std::unique_ptr<ui::ComboboxModel> combobox_model,
                            bool filter_on_edit,
                            bool show_on_empty,
                            int text_context = kDefaultTextContext,
                            int text_style = kDefaultTextStyle);

  ~EditableCombobox() override;

  // Gets the text currently in the textfield.
  const base::string16& GetText() const;

  const gfx::FontList& GetFontList() const;

  // Sets the listener that we will call when a selection is made.
  void set_listener(EditableComboboxListener* listener) {
    listener_ = listener;
  }

  // Sets the accessible name. Use SetAssociatedLabel instead if there is a
  // label associated with this combobox.
  void SetAccessibleName(const base::string16& name);

  // Sets the associated label; use this instead of SetAccessibleName if there
  // is a label associated with this combobox.
  void SetAssociatedLabel(View* labelling_view);

  // Accessors of private members for tests.
  ui::ComboboxModel* GetComboboxModelForTest() { return combobox_model_.get(); }
  int GetItemCountForTest();
  base::string16 GetItemForTest(int index);
  MenuRunner* GetMenuRunnerForTest() { return menu_runner_.get(); }
  Textfield* GetTextfieldForTest() { return textfield_; }
  void SetTextForTest(const base::string16& text);

 private:
  class EditableComboboxMenuModel;

  // Called when an item is selected from the menu.
  void OnItemSelected(int index);

  // Notifies listener of new content and updates the menu items to show.
  void HandleNewContent(const base::string16& new_content);

  // Cleans up after the menu is closed.
  void OnMenuClosed();

  // Shows the drop-down menu.
  void ShowDropDownMenu(ui::MenuSourceType source_type = ui::MENU_SOURCE_NONE);

  // Overridden from View:
  const char* GetClassName() const override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // Overridden from TextfieldController:
  void ContentsChanged(Textfield* sender,
                       const base::string16& new_contents) override;

  // Overridden from ViewObserver:
  void OnViewFocused(View* observed_view) override;
  void OnViewBlurred(View* observed_view) override;

  std::unique_ptr<ui::ComboboxModel> combobox_model_;

  // The EditableComboboxMenuModel used by |menu_runner_|.
  std::unique_ptr<EditableComboboxMenuModel> menu_model_;

  // Typography context for the text written in the textfield and the options
  // shown in the drop-down menu.
  const int text_context_;

  // Typography style for the text written in the textfield and the options
  // shown in the drop-down menu.
  const int text_style_;

  Textfield* textfield_;

  // Set while the drop-down is showing.
  std::unique_ptr<MenuRunner> menu_runner_;

  // Our listener. Not owned. Notified when the selected index changes.
  EditableComboboxListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(EditableCombobox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_EDITABLE_COMBOBOX_EDITABLE_COMBOBOX_H_
