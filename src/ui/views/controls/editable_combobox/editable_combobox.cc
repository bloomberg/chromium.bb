// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
const char EditableCombobox::kViewClassName[] = "EditableCombobox";

// Adapts a ui::ComboboxModel to a ui::MenuModel to be used by EditableCombobox.
// Also provides a filtering capability.
class EditableCombobox::EditableComboboxMenuModel
    : public ui::MenuModel,
      public ui::ComboboxModelObserver {
 public:
  EditableComboboxMenuModel(EditableCombobox* owner,
                            ui::ComboboxModel* combobox_model,
                            const bool filter_on_edit,
                            const bool show_on_empty)
      : owner_(owner),
        combobox_model_(combobox_model),
        filter_on_edit_(filter_on_edit),
        show_on_empty_(show_on_empty) {
    UpdateItemsShown(base::string16());
    combobox_model_->AddObserver(this);
  }

  ~EditableComboboxMenuModel() override {
    combobox_model_->RemoveObserver(this);
  }

  void UpdateItemsShown(const base::string16& text) {
    if (!update_items_shown_enabled_)
      return;
    text_ = text;
    items_shown_.clear();
    items_shown_enabled_.clear();
    if (show_on_empty_ || !text.empty()) {
      for (int i = 0; i < combobox_model_->GetItemCount(); ++i) {
        if (!filter_on_edit_ ||
            base::StartsWith(combobox_model_->GetItemAt(i), text,
                             base::CompareCase::INSENSITIVE_ASCII)) {
          items_shown_.push_back(combobox_model_->GetItemAt(i));
          items_shown_enabled_.push_back(combobox_model_->IsItemEnabledAt(i));
        }
      }
    }
    if (menu_model_delegate())
      menu_model_delegate()->OnMenuStructureChanged();
  }

  void DisableUpdateItemsShown() { update_items_shown_enabled_ = false; }

  void EnableUpdateItemsShown() { update_items_shown_enabled_ = true; }

  //////////////////////////////////////////////////////////////////////////////
  // Overridden from ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    UpdateItemsShown(text_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Overridden from MenuModel:

  int GetItemCount() const override { return items_shown_.size(); }

  base::string16 GetLabelAt(int index) const override {
    // Inserting the Unicode formatting characters if necessary so that the text
    // is displayed correctly in right-to-left UIs.
    base::string16 text = items_shown_[index];
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

 private:
  bool HasIcons() const override { return false; }

  ItemType GetTypeAt(int index) const override { return TYPE_COMMAND; }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    constexpr int kFirstMenuItemId = 1000;
    return index + kFirstMenuItemId;
  }

  bool IsItemDynamicAt(int index) const override { return false; }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return &owner_->GetFontList();
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(int index) const override { return false; }

  int GetGroupIdAt(int index) const override { return -1; }

  bool GetIconAt(int index, gfx::Image* icon) override { return false; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override {
    return items_shown_enabled_[index];
  }

  void ActivatedAt(int index) override { owner_->OnItemSelected(index); }

  MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  EditableCombobox* owner_;            // Weak. Owns |this|.
  ui::ComboboxModel* combobox_model_;  // Weak.

  // Whether to adapt the items shown to the textfield content.
  const bool filter_on_edit_;

  // Whether to show options when the textfield is empty.
  const bool show_on_empty_;

  // The items from |combobox_model_| that we are currently showing.
  std::vector<base::string16> items_shown_;
  std::vector<bool> items_shown_enabled_;

  // The current content of |owner_|'s textfield.
  base::string16 text_;

  // When false, UpdateItemsShown doesn't do anything.
  bool update_items_shown_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(EditableComboboxMenuModel);
};

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, public, non-overridden methods:
EditableCombobox::EditableCombobox(
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const bool filter_on_edit,
    const bool show_on_empty,
    const int text_context,
    const int text_style)
    : combobox_model_(std::move(combobox_model)),
      menu_model_(
          std::make_unique<EditableComboboxMenuModel>(this,
                                                      combobox_model_.get(),
                                                      filter_on_edit,
                                                      show_on_empty)),
      text_context_(text_context),
      text_style_(text_style),
      textfield_(new Textfield()),
      listener_(nullptr) {
  textfield_->AddObserver(this);
  textfield_->set_controller(this);
  AddChildView(textfield_);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

EditableCombobox::~EditableCombobox() {
  textfield_->set_controller(nullptr);
  textfield_->RemoveObserver(this);
}

const base::string16& EditableCombobox::GetText() const {
  return textfield_->text();
}

const gfx::FontList& EditableCombobox::GetFontList() const {
  return style::GetFont(text_context_, text_style_);
}

void EditableCombobox::SetAccessibleName(const base::string16& name) {
  textfield_->SetAccessibleName(name);
}

void EditableCombobox::SetAssociatedLabel(View* labelling_view) {
  textfield_->SetAssociatedLabel(labelling_view);
}

int EditableCombobox::GetItemCountForTest() {
  return menu_model_->GetItemCount();
}

base::string16 EditableCombobox::GetItemForTest(int index) {
  return menu_model_->GetLabelAt(index);
}

void EditableCombobox::SetTextForTest(const base::string16& text) {
  textfield_->SetText(text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(text);
  ShowDropDownMenu();
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, View overrides:

const char* EditableCombobox::GetClassName() const {
  return kViewClassName;
}

void EditableCombobox::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  textfield_->OnNativeThemeChanged(theme);
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, TextfieldController overrides:

void EditableCombobox::ContentsChanged(Textfield* sender,
                                       const base::string16& new_contents) {
  HandleNewContent(new_contents);
  ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
}

void EditableCombobox::OnViewBlurred(View* observed_view) {
  menu_runner_.reset();
}

void EditableCombobox::OnViewFocused(View* observed_view) {
  ShowDropDownMenu();
}

////////////////////////////////////////////////////////////////////////////////
// EditableCombobox, Private methods:

void EditableCombobox::OnItemSelected(int index) {
  textfield_->SetText(menu_model_->GetLabelAt(index));
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(menu_model_->GetLabelAt(index));
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged,
                           /*xsend_native_event=*/true);
}

void EditableCombobox::HandleNewContent(const base::string16& new_content) {
  DCHECK(GetText() == new_content);
  // We notify |listener_| before updating |menu_model_|'s items shown. This
  // gives the user a chance to modify the ComboboxModel beforehand if they wish
  // to do so.
  // We disable UpdateItemsShown while we notify the listener in case it
  // modifies the ComboboxModel, then calls OnComboboxModelChanged and thus
  // UpdateItemsShown. That way UpdateItemsShown doesn't do its work twice.
  if (listener_) {
    menu_model_->DisableUpdateItemsShown();
    listener_->OnContentChanged(this);
    menu_model_->EnableUpdateItemsShown();
  }
  menu_model_->UpdateItemsShown(new_content);
}

void EditableCombobox::OnMenuClosed() {
  menu_runner_.reset();
}

void EditableCombobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  constexpr int kMenuBorderWidthLeft = 1;
  constexpr int kMenuBorderWidthTop = 1;
  constexpr int kMenuBorderWidthRight = 1;

  if (!menu_model_->GetItemCount()) {
    menu_runner_.reset();
    return;
  }
  if (!textfield_->HasFocus() || (menu_runner_ && menu_runner_->IsRunning()))
    return;

  gfx::Rect local_bounds = textfield_->GetLocalBounds();
  gfx::Point menu_position(local_bounds.origin());
  // Inset the menu's requested position so the border of the menu lines up
  // with the border of the textfield.
  menu_position.set_x(menu_position.x() + kMenuBorderWidthLeft);
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);
  local_bounds.set_width(local_bounds.width() -
                         (kMenuBorderWidthLeft + kMenuBorderWidthRight));
  View::ConvertPointToScreen(this, &menu_position);
  gfx::Rect bounds(menu_position, local_bounds.size());

  menu_runner_ = std::make_unique<MenuRunner>(
      menu_model_.get(), MenuRunner::EDITABLE_COMBOBOX,
      base::BindRepeating(&EditableCombobox::OnMenuClosed,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds, MENU_ANCHOR_TOPLEFT,
                          source_type);
}

}  // namespace views
