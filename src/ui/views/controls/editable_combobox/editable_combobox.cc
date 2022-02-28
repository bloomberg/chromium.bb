// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/combobox/empty_combobox_model.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

class Arrow : public Button {
 public:
  METADATA_HEADER(Arrow);

  explicit Arrow(PressedCallback callback) : Button(std::move(callback)) {
    // Similar to Combobox's TransparentButton.
    SetFocusBehavior(FocusBehavior::NEVER);
    button_controller()->set_notify_action(
        ButtonController::NotifyAction::kOnPress);

    // TODO(pbos): Share ink-drop configuration code between here and
    // Combobox's TransparentButton.
    // Similar to Combobox's TransparentButton.
    InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    InkDrop::UseInkDropForSquareRipple(InkDrop::Get(this),
                                       /*highlight_on_hover=*/false);
    InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
        [](Button* host) -> std::unique_ptr<views::InkDropRipple> {
          return std::make_unique<views::FloodFillInkDropRipple>(
              host->size(),
              InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
              style::GetColor(*host, style::CONTEXT_TEXTFIELD,
                              style::STYLE_PRIMARY),
              InkDrop::Get(host)->GetVisibleOpacity());
        },
        this));
  }
  Arrow(const Arrow&) = delete;
  Arrow& operator=(const Arrow&) = delete;
  ~Arrow() override = default;

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }

 private:
  // Button:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->ClipRect(GetContentsBounds());
    gfx::Rect arrow_bounds = GetLocalBounds();
    arrow_bounds.ClampToCenteredSize(ComboboxArrowSize());
    // Make sure the arrow use the same color as the text in the combobox.
    PaintComboboxArrow(style::GetColor(*this, style::CONTEXT_TEXTFIELD,
                                       GetEnabled() ? style::STYLE_PRIMARY
                                                    : style::STYLE_DISABLED),
                       arrow_bounds, canvas);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kComboBoxMenuButton;
    node_data->SetName(GetAccessibleName());
    node_data->SetHasPopup(ax::mojom::HasPopup::kMenu);
    if (GetEnabled())
      node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
  }
};

BEGIN_METADATA(Arrow, Button)
END_METADATA

}  // namespace

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
    UpdateItemsShown();
    observation_.Observe(combobox_model_.get());
  }

  EditableComboboxMenuModel(const EditableComboboxMenuModel&) = delete;
  EditableComboboxMenuModel& operator=(const EditableComboboxMenuModel&) =
      delete;

  ~EditableComboboxMenuModel() override = default;

  void UpdateItemsShown() {
    if (!update_items_shown_enabled_)
      return;
    items_shown_.clear();
    if (show_on_empty_ || !owner_->GetText().empty()) {
      for (int i = 0; i < combobox_model_->GetItemCount(); ++i) {
        if (!filter_on_edit_ ||
            base::StartsWith(combobox_model_->GetItemAt(i), owner_->GetText(),
                             base::CompareCase::INSENSITIVE_ASCII)) {
          items_shown_.push_back(
              {static_cast<size_t>(i), combobox_model_->IsItemEnabledAt(i)});
        }
      }
    }
    if (menu_model_delegate())
      menu_model_delegate()->OnMenuStructureChanged();
  }

  void DisableUpdateItemsShown() { update_items_shown_enabled_ = false; }

  void EnableUpdateItemsShown() { update_items_shown_enabled_ = true; }

  bool UseCheckmarks() const {
    return MenuConfig::instance().check_selected_combobox_item;
  }

  std::u16string GetItemTextAt(int index, bool showing_password_text) const {
    int index_in_model = items_shown_[index].index;
    std::u16string text = combobox_model_->GetItemAt(index_in_model);
    return showing_password_text
               ? text
               : std::u16string(text.length(),
                                gfx::RenderText::kPasswordReplacementChar);
  }

  ui::ImageModel GetIconAt(int index) const override {
    return combobox_model_->GetDropDownIconAt(items_shown_[index].index);
  }

  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    UpdateItemsShown();
  }

  int GetItemCount() const override { return items_shown_.size(); }

 private:
  struct ShownItem {
    size_t index;
    bool enabled;
  };
  bool HasIcons() const override {
    for (int i = 0; i < GetItemCount(); ++i) {
      if (!GetIconAt(i).IsEmpty())
        return true;
    }
    return false;
  }

  ItemType GetTypeAt(int index) const override {
    return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    constexpr int kFirstMenuItemId = 1000;
    return index + kFirstMenuItemId;
  }

  std::u16string GetLabelAt(int index) const override {
    std::u16string text = GetItemTextAt(index, owner_->showing_password_text_);
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

  bool IsItemDynamicAt(int index) const override { return false; }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return &owner_->GetFontList();
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(int index) const override {
    return UseCheckmarks() &&
           combobox_model_->GetItemAt(items_shown_[index].index) ==
               owner_->GetText();
  }

  int GetGroupIdAt(int index) const override { return -1; }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override {
    return items_shown_[index].enabled;
  }

  void ActivatedAt(int index) override { owner_->OnItemSelected(index); }

  MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  raw_ptr<EditableCombobox> owner_;            // Weak. Owns |this|.
  raw_ptr<ui::ComboboxModel> combobox_model_;  // Weak.

  // Whether to adapt the items shown to the textfield content.
  const bool filter_on_edit_;

  // Whether to show options when the textfield is empty.
  const bool show_on_empty_;

  // The indices of the items from |combobox_model_| that we are currently
  // showing, and whether they are enabled.
  std::vector<ShownItem> items_shown_;

  // When false, UpdateItemsShown doesn't do anything.
  bool update_items_shown_enabled_ = true;

  base::ScopedObservation<ui::ComboboxModel, ui::ComboboxModelObserver>
      observation_{this};
};

// This class adds itself as the pre-target handler for the RootView of the
// EditableCombobox. We use it to close the menu when press events happen in the
// RootView but not inside the EditableComboobox's textfield.
class EditableCombobox::EditableComboboxPreTargetHandler
    : public ui::EventHandler {
 public:
  EditableComboboxPreTargetHandler(EditableCombobox* owner, View* root_view)
      : owner_(owner), root_view_(root_view) {
    root_view_->AddPreTargetHandler(this);
  }

  EditableComboboxPreTargetHandler(const EditableComboboxPreTargetHandler&) =
      delete;
  EditableComboboxPreTargetHandler& operator=(
      const EditableComboboxPreTargetHandler&) = delete;

  ~EditableComboboxPreTargetHandler() override { StopObserving(); }

  // ui::EventHandler overrides.
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED &&
        event->button_flags() == event->changed_button_flags())
      HandlePressEvent(event->root_location());
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (event->type() == ui::ET_TOUCH_PRESSED)
      HandlePressEvent(event->root_location());
  }

 private:
  void HandlePressEvent(const gfx::Point& root_location) {
    View* handler = root_view_->GetEventHandlerForPoint(root_location);
    if (handler == owner_->textfield_ || handler == owner_->arrow_)
      return;
    owner_->CloseMenu();
  }

  void StopObserving() {
    if (!root_view_)
      return;
    root_view_->RemovePreTargetHandler(this);
    root_view_ = nullptr;
  }

  raw_ptr<EditableCombobox> owner_;
  raw_ptr<View> root_view_;
};

EditableCombobox::EditableCombobox()
    : EditableCombobox(std::make_unique<internal::EmptyComboboxModel>()) {}

EditableCombobox::EditableCombobox(
    std::unique_ptr<ui::ComboboxModel> combobox_model,
    const bool filter_on_edit,
    const bool show_on_empty,
    const Type type,
    const int text_context,
    const int text_style,
    const bool display_arrow)
    : textfield_(new Textfield()),
      text_context_(text_context),
      text_style_(text_style),
      type_(type),
      filter_on_edit_(filter_on_edit),
      show_on_empty_(show_on_empty),
      showing_password_text_(type != Type::kPassword) {
  SetModel(std::move(combobox_model));
  observation_.Observe(textfield_.get());
  textfield_->set_controller(this);
  textfield_->SetFontList(GetFontList());
  textfield_->SetTextInputType((type == Type::kPassword)
                                   ? ui::TEXT_INPUT_TYPE_PASSWORD
                                   : ui::TEXT_INPUT_TYPE_TEXT);
  AddChildView(textfield_.get());
  if (display_arrow) {
    textfield_->SetExtraInsets(gfx::Insets(
        /*top=*/0, /*left=*/0, /*bottom=*/0,
        /*right=*/kComboboxArrowContainerWidth - kComboboxArrowPaddingWidth));
    arrow_ = AddChildView(std::make_unique<Arrow>(base::BindRepeating(
        &EditableCombobox::ArrowButtonPressed, base::Unretained(this))));
  }
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

EditableCombobox::~EditableCombobox() {
  CloseMenu();
  textfield_->set_controller(nullptr);
}

void EditableCombobox::SetModel(std::unique_ptr<ui::ComboboxModel> model) {
  CloseMenu();
  combobox_model_.swap(model);
  menu_model_ = std::make_unique<EditableComboboxMenuModel>(
      this, combobox_model_.get(), filter_on_edit_, show_on_empty_);
}

const std::u16string& EditableCombobox::GetText() const {
  return textfield_->GetText();
}

void EditableCombobox::SetText(const std::u16string& text) {
  textfield_->SetText(text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(text);
}

const gfx::FontList& EditableCombobox::GetFontList() const {
  return style::GetFont(text_context_, text_style_);
}

void EditableCombobox::SelectRange(const gfx::Range& range) {
  textfield_->SetSelectedRange(range);
}

void EditableCombobox::SetAccessibleName(const std::u16string& name) {
  textfield_->SetAccessibleName(name);
  if (arrow_)
    arrow_->SetAccessibleName(name);
}

void EditableCombobox::SetAssociatedLabel(View* labelling_view) {
  textfield_->SetAssociatedLabel(labelling_view);
}

void EditableCombobox::RevealPasswords(bool revealed) {
  DCHECK_EQ(Type::kPassword, type_);
  if (revealed == showing_password_text_)
    return;
  showing_password_text_ = revealed;
  textfield_->SetTextInputType(revealed ? ui::TEXT_INPUT_TYPE_TEXT
                                        : ui::TEXT_INPUT_TYPE_PASSWORD);
  menu_model_->UpdateItemsShown();
}

int EditableCombobox::GetItemCountForTest() {
  return menu_model_->GetItemCount();
}

std::u16string EditableCombobox::GetItemForTest(int index) {
  return menu_model_->GetItemTextAt(index, showing_password_text_);
}

ui::ImageModel EditableCombobox::GetIconForTest(int index) {
  return menu_model_->GetIconAt(index);
}

void EditableCombobox::Layout() {
  View::Layout();
  if (arrow_) {
    gfx::Rect arrow_bounds(/*x=*/width() - kComboboxArrowContainerWidth,
                           /*y=*/0, kComboboxArrowContainerWidth, height());
    arrow_->SetBoundsRect(arrow_bounds);
  }
}

void EditableCombobox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kComboBoxGrouping;

  node_data->SetName(textfield_->GetAccessibleName());
  node_data->SetValue(GetText());
}

void EditableCombobox::RequestFocus() {
  textfield_->RequestFocus();
}

bool EditableCombobox::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void EditableCombobox::OnVisibleBoundsChanged() {
  CloseMenu();
}

void EditableCombobox::ContentsChanged(Textfield* sender,
                                       const std::u16string& new_contents) {
  HandleNewContent(new_contents);
  ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
}

bool EditableCombobox::HandleKeyEvent(Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  if (key_event.type() == ui::ET_KEY_PRESSED &&
      (key_event.key_code() == ui::VKEY_UP ||
       key_event.key_code() == ui::VKEY_DOWN)) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return false;
}

void EditableCombobox::OnViewBlurred(View* observed_view) {
  CloseMenu();
}

void EditableCombobox::OnLayoutIsAnimatingChanged(
    views::AnimatingLayoutManager* source,
    bool is_animating) {
  dropdown_blocked_for_animation_ = is_animating;
  if (dropdown_blocked_for_animation_)
    CloseMenu();
}

void EditableCombobox::CloseMenu() {
  menu_runner_.reset();
  pre_target_handler_.reset();
}

void EditableCombobox::OnItemSelected(int index) {
  // |textfield_| can hide the characters on its own so we read the actual
  // characters instead of gfx::RenderText::kPasswordReplacementChar characters.
  std::u16string selected_item_text =
      menu_model_->GetItemTextAt(index, /*showing_password_text=*/true);
  textfield_->SetText(selected_item_text);
  // SetText does not actually notify the TextfieldController, so we call the
  // handling code directly.
  HandleNewContent(selected_item_text);
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged,
                           /*send_native_event=*/true);
}

void EditableCombobox::HandleNewContent(const std::u16string& new_content) {
  DCHECK(GetText() == new_content);
  // We notify |callback_| before updating |menu_model_|'s items shown. This
  // gives the user a chance to modify the ComboboxModel beforehand if they wish
  // to do so.
  // We disable UpdateItemsShown while we notify the listener in case it
  // modifies the ComboboxModel, then calls OnComboboxModelChanged and thus
  // UpdateItemsShown. That way UpdateItemsShown doesn't do its work twice.
  if (!content_changed_callback_.is_null()) {
    menu_model_->DisableUpdateItemsShown();
    content_changed_callback_.Run();
    menu_model_->EnableUpdateItemsShown();
  }
  menu_model_->UpdateItemsShown();
}

void EditableCombobox::ArrowButtonPressed(const ui::Event& event) {
  textfield_->RequestFocus();
  if (menu_runner_ && menu_runner_->IsRunning())
    CloseMenu();
  else
    ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
}

void EditableCombobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  constexpr int kMenuBorderWidthTop = 1;

  if (dropdown_blocked_for_animation_)
    return;

  if (!menu_model_->GetItemCount()) {
    CloseMenu();
    return;
  }
  if (menu_runner_ && menu_runner_->IsRunning())
    return;
  if (!GetWidget())
    return;

  // Since we don't capture the mouse, we want to see the events that happen in
  // the EditableCombobox's RootView to get a chance to close the menu if they
  // happen outside |textfield_|. Events that happen over the menu belong to
  // another Widget and they don't go through this pre-target handler.
  // Events that happen outside both the menu and the RootView cause
  // OnViewBlurred to be called, which also closes the menu.
  pre_target_handler_ = std::make_unique<EditableComboboxPreTargetHandler>(
      this, GetWidget()->GetRootView());

  gfx::Rect local_bounds = textfield_->GetLocalBounds();

  // Menu's requested position's width should be the same as local bounds so the
  // border of the menu lines up with the border of the combobox. The y
  // coordinate however should be shifted to the bottom with the border width
  // not to overlap with the combobox border.
  gfx::Point menu_position(local_bounds.origin());
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);
  View::ConvertPointToScreen(this, &menu_position);
  gfx::Rect bounds(menu_position, local_bounds.size());

  menu_runner_ = std::make_unique<MenuRunner>(
      menu_model_.get(), MenuRunner::EDITABLE_COMBOBOX,
      base::BindRepeating(&EditableCombobox::CloseMenu,
                          base::Unretained(this)));
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds,
                          MenuAnchorPosition::kTopLeft, source_type);
}

BEGIN_METADATA(EditableCombobox, View)
END_METADATA

}  // namespace views
