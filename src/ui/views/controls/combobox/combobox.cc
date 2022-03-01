// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/combobox/combobox_util.h"
#include "ui/views/controls/combobox/empty_combobox_model.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/image_model_utils.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Used to indicate that no item is currently selected by the user.
constexpr int kNoSelection = -1;

SkColor GetTextColorForEnableState(const Combobox& combobox, bool enabled) {
  const int style = enabled ? style::STYLE_PRIMARY : style::STYLE_DISABLED;
  return style::GetColor(combobox, style::CONTEXT_TEXTFIELD, style);
}

// The transparent button which holds a button state but is not rendered.
class TransparentButton : public Button {
 public:
  explicit TransparentButton(PressedCallback callback)
      : Button(std::move(callback)) {
    SetFocusBehavior(FocusBehavior::NEVER);
    button_controller()->set_notify_action(
        ButtonController::NotifyAction::kOnPress);

    InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    InkDrop::UseInkDropForSquareRipple(InkDrop::Get(this),
                                       /*highlight_on_hover=*/false);
    InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
        [](Button* host) -> std::unique_ptr<views::InkDropRipple> {
          return std::make_unique<views::FloodFillInkDropRipple>(
              host->size(),
              InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
              host->GetColorProvider()->GetColor(ui::kColorLabelForeground),
              InkDrop::Get(host)->GetVisibleOpacity());
        },
        this));
  }
  TransparentButton(const TransparentButton&) = delete;
  TransparentButton& operator&=(const TransparentButton&) = delete;
  ~TransparentButton() override = default;

  bool OnMousePressed(const ui::MouseEvent& mouse_event) override {
#if !defined(OS_MAC)
    // On Mac, comboboxes do not take focus on mouse click, but on other
    // platforms they do.
    parent()->RequestFocus();
#endif
    return Button::OnMousePressed(mouse_event);
  }

  double GetAnimationValue() const {
    return hover_animation().GetCurrentValue();
  }
};

#if !defined(OS_MAC)
// Returns the next or previous valid index (depending on |increment|'s value).
// Skips separator or disabled indices. Returns -1 if there is no valid adjacent
// index.
int GetAdjacentIndex(ui::ComboboxModel* model, int increment, int index) {
  DCHECK(increment == -1 || increment == 1);

  index += increment;
  while (index >= 0 && index < model->GetItemCount()) {
    if (!model->IsItemSeparatorAt(index) || !model->IsItemEnabledAt(index))
      return index;
    index += increment;
  }
  return kNoSelection;
}
#endif

}  // namespace

// Adapts a ui::ComboboxModel to a ui::MenuModel.
class Combobox::ComboboxMenuModel : public ui::MenuModel {
 public:
  ComboboxMenuModel(Combobox* owner, ui::ComboboxModel* model)
      : owner_(owner), model_(model) {}
  ComboboxMenuModel(const ComboboxMenuModel&) = delete;
  ComboboxMenuModel& operator&(const ComboboxMenuModel&) = delete;
  ~ComboboxMenuModel() override = default;

 private:
  bool UseCheckmarks() const {
    return MenuConfig::instance().check_selected_combobox_item;
  }

  // Overridden from MenuModel:
  bool HasIcons() const override {
    for (int i = 0; i < GetItemCount(); ++i) {
      if (!GetIconAt(i).IsEmpty())
        return true;
    }
    return false;
  }

  int GetItemCount() const override { return model_->GetItemCount(); }

  ItemType GetTypeAt(int index) const override {
    if (model_->IsItemSeparatorAt(index))
      return TYPE_SEPARATOR;
    return UseCheckmarks() ? TYPE_CHECK : TYPE_COMMAND;
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    // Define the id of the first item in the menu (since it needs to be > 0)
    constexpr int kFirstMenuItemId = 1000;
    return index + kFirstMenuItemId;
  }

  std::u16string GetLabelAt(int index) const override {
    // Inserting the Unicode formatting characters if necessary so that the
    // text is displayed correctly in right-to-left UIs.
    std::u16string text = model_->GetDropDownTextAt(index);
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

  std::u16string GetSecondaryLabelAt(int index) const override {
    std::u16string text = model_->GetDropDownSecondaryTextAt(index);
    base::i18n::AdjustStringForLocaleDirection(&text);
    return text;
  }

  bool IsItemDynamicAt(int index) const override { return true; }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return &owner_->GetFontList();
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }

  bool IsItemCheckedAt(int index) const override {
    return UseCheckmarks() && index == owner_->selected_index_;
  }

  int GetGroupIdAt(int index) const override { return -1; }

  ui::ImageModel GetIconAt(int index) const override {
    return model_->GetDropDownIconAt(index);
  }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }

  bool IsEnabledAt(int index) const override {
    return model_->IsItemEnabledAt(index);
  }

  void ActivatedAt(int index) override {
    owner_->SetSelectedIndex(index);
    owner_->OnPerformAction();
  }

  void ActivatedAt(int index, int event_flags) override { ActivatedAt(index); }

  MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }

  raw_ptr<Combobox> owner_;           // Weak. Owns this.
  raw_ptr<ui::ComboboxModel> model_;  // Weak.
};

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox(int text_context, int text_style)
    : Combobox(std::make_unique<internal::EmptyComboboxModel>()) {}

Combobox::Combobox(std::unique_ptr<ui::ComboboxModel> model,
                   int text_context,
                   int text_style)
    : Combobox(model.get(), text_context, text_style) {
  owned_model_ = std::move(model);
}

Combobox::Combobox(ui::ComboboxModel* model, int text_context, int text_style)
    : text_context_(text_context),
      text_style_(text_style),
      arrow_button_(new TransparentButton(
          base::BindRepeating(&Combobox::ArrowButtonPressed,
                              base::Unretained(this)))) {
  SetModel(model);
#if defined(OS_MAC)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif

  UpdateBorder();

  arrow_button_->SetVisible(true);
  AddChildView(arrow_button_.get());

  // A layer is applied to make sure that canvas bounds are snapped to pixel
  // boundaries (for the sake of drawing the arrow).
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  FocusRing::Install(this);
}

Combobox::~Combobox() {
  if (GetInputMethod() && selector_.get()) {
    // Combobox should have been blurred before destroy.
    DCHECK(selector_.get() != GetInputMethod()->GetTextInputClient());
  }
}

const gfx::FontList& Combobox::GetFontList() const {
  return style::GetFont(text_context_, text_style_);
}

void Combobox::SetSelectedIndex(int index) {
  if (selected_index_ == index)
    return;
  // TODO(pbos): Add (D)CHECKs to validate the selected index.
  selected_index_ = index;
  if (size_to_largest_label_) {
    OnPropertyChanged(&selected_index_, kPropertyEffectsPaint);
  } else {
    content_size_ = GetContentSize();
    OnPropertyChanged(&selected_index_, kPropertyEffectsPreferredSizeChanged);
  }
}

base::CallbackListSubscription Combobox::AddSelectedIndexChangedCallback(
    views::PropertyChangedCallback callback) {
  return AddPropertyChangedCallback(&selected_index_, std::move(callback));
}

bool Combobox::SelectValue(const std::u16string& value) {
  for (int i = 0; i < GetModel()->GetItemCount(); ++i) {
    if (value == GetModel()->GetItemAt(i)) {
      SetSelectedIndex(i);
      return true;
    }
  }
  return false;
}

void Combobox::SetOwnedModel(std::unique_ptr<ui::ComboboxModel> model) {
  // The swap keeps the outgoing model alive for SetModel().
  owned_model_.swap(model);
  SetModel(owned_model_.get());
}

void Combobox::SetModel(ui::ComboboxModel* model) {
  if (!model) {
    SetOwnedModel(std::make_unique<internal::EmptyComboboxModel>());
    return;
  }

  if (model_) {
    DCHECK(observation_.IsObservingSource(model_.get()));
    observation_.Reset();
  }

  model_ = model;

  if (model_) {
    model_ = model;
    menu_model_ = std::make_unique<ComboboxMenuModel>(this, model_);
    observation_.Observe(model_.get());
    SetSelectedIndex(model_->GetDefaultIndex());
    OnComboboxModelChanged(model_);
  }
}

std::u16string Combobox::GetTooltipTextAndAccessibleName() const {
  return arrow_button_->GetTooltipText();
}

void Combobox::SetTooltipTextAndAccessibleName(
    const std::u16string& tooltip_text) {
  arrow_button_->SetTooltipText(tooltip_text);
  if (accessible_name_.empty())
    accessible_name_ = tooltip_text;
}

void Combobox::SetAccessibleName(const std::u16string& name) {
  accessible_name_ = name;
}

std::u16string Combobox::GetAccessibleName() const {
  return accessible_name_;
}

void Combobox::SetInvalid(bool invalid) {
  if (invalid == invalid_)
    return;

  invalid_ = invalid;

  if (views::FocusRing::Get(this))
    views::FocusRing::Get(this)->SetInvalid(invalid);

  UpdateBorder();
  OnPropertyChanged(&selected_index_, kPropertyEffectsPaint);
}

void Combobox::SetSizeToLargestLabel(bool size_to_largest_label) {
  if (size_to_largest_label_ == size_to_largest_label)
    return;

  size_to_largest_label_ = size_to_largest_label;
  content_size_ = GetContentSize();
  OnPropertyChanged(&selected_index_, kPropertyEffectsPreferredSizeChanged);
}

bool Combobox::IsMenuRunning() const {
  return menu_runner_ && menu_runner_->IsRunning();
}

void Combobox::OnThemeChanged() {
  View::OnThemeChanged();
  SetBackground(
      CreateBackgroundFromPainter(Painter::CreateSolidRoundRectPainter(
          GetColorProvider()->GetColor(ui::kColorTextfieldBackground),
          FocusableBorder::kCornerRadiusDp)));
  OnContentSizeMaybeChanged();
}

int Combobox::GetRowCount() {
  return GetModel()->GetItemCount();
}

int Combobox::GetSelectedRow() {
  return selected_index_;
}

void Combobox::SetSelectedRow(int row) {
  int prev_index = selected_index_;
  SetSelectedIndex(row);
  if (selected_index_ != prev_index)
    OnPerformAction();
}

std::u16string Combobox::GetTextForRow(int row) {
  return GetModel()->IsItemSeparatorAt(row) ? std::u16string()
                                            : GetModel()->GetItemAt(row);
}

////////////////////////////////////////////////////////////////////////////////
// Combobox, View overrides:

gfx::Size Combobox::CalculatePreferredSize() const {
  // Limit how small a combobox can be.
  constexpr int kMinComboboxWidth = 25;

  // The preferred size will drive the local bounds which in turn is used to set
  // the minimum width for the dropdown list.
  const int width = std::max(kMinComboboxWidth, content_size_.width()) +
                    LayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING) *
                        2 +
                    kComboboxArrowContainerWidth + GetInsets().width();
  const int height = LayoutProvider::GetControlHeightForFont(
      text_context_, text_style_, GetFontList());
  return gfx::Size(width, height);
}

void Combobox::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  arrow_button_->SetBounds(0, 0, width(), height());
}

bool Combobox::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Escape should close the drop down list when it is active, not host UI.
  if (e.key_code() != ui::VKEY_ESCAPE || e.IsShiftDown() || e.IsControlDown() ||
      e.IsAltDown() || e.IsAltGrDown()) {
    return false;
  }
  return !!menu_runner_;
}

bool Combobox::OnKeyPressed(const ui::KeyEvent& e) {
  // TODO(oshima): handle IME.
  DCHECK_EQ(e.type(), ui::ET_KEY_PRESSED);

  // TODO(pbos): Do we need to handle selected_index_ == -1 for unselected here?
  // Ditto on handling an empty model?
  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, GetModel()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ >= GetModel()->GetItemCount())
    SetSelectedIndex(0);

  bool show_menu = false;
  int new_index = kNoSelection;
  switch (e.key_code()) {
#if defined(OS_MAC)
    case ui::VKEY_DOWN:
    case ui::VKEY_UP:
    case ui::VKEY_SPACE:
    case ui::VKEY_HOME:
    case ui::VKEY_END:
      // On Mac, navigation keys should always just show the menu first.
      show_menu = true;
      break;
#else
    // Show the menu on F4 without modifiers.
    case ui::VKEY_F4:
      if (e.IsAltDown() || e.IsAltGrDown() || e.IsControlDown())
        return false;
      show_menu = true;
      break;

    // Move to the next item if any, or show the menu on Alt+Down like Windows.
    case ui::VKEY_DOWN:
      if (e.IsAltDown())
        show_menu = true;
      else
        new_index = GetAdjacentIndex(GetModel(), 1, selected_index_);
      break;

    // Move to the end of the list.
    case ui::VKEY_END:
    case ui::VKEY_NEXT:  // Page down.
      new_index = GetAdjacentIndex(GetModel(), -1, GetModel()->GetItemCount());
      break;

    // Move to the beginning of the list.
    case ui::VKEY_HOME:
    case ui::VKEY_PRIOR:  // Page up.
      new_index = GetAdjacentIndex(GetModel(), 1, -1);
      break;

    // Move to the previous item if any.
    case ui::VKEY_UP:
      new_index = GetAdjacentIndex(GetModel(), -1, selected_index_);
      break;

    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
      show_menu = true;
      break;
#endif  // OS_MAC
    default:
      return false;
  }

  if (show_menu) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
  } else if (new_index != selected_index_ && new_index != kNoSelection) {
    DCHECK(!GetModel()->IsItemSeparatorAt(new_index));
    SetSelectedIndex(new_index);
    OnPerformAction();
  }

  return true;
}

void Combobox::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintIconAndText(canvas);
  OnPaintBorder(canvas);
}

void Combobox::OnFocus() {
  if (GetInputMethod())
    GetInputMethod()->SetFocusedTextInputClient(GetPrefixSelector());

  View::OnFocus();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::OnBlur() {
  if (GetInputMethod())
    GetInputMethod()->DetachTextInputClient(GetPrefixSelector());

  if (selector_)
    selector_->OnViewBlur();
  // Border renders differently when focused.
  SchedulePaint();
}

void Combobox::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // ax::mojom::Role::kComboBox is for UI elements with a dropdown and
  // an editable text field, which views::Combobox does not have. Use
  // ax::mojom::Role::kPopUpButton to match an HTML <select> element.
  node_data->role = ax::mojom::Role::kPopUpButton;
  if (menu_runner_) {
    node_data->AddState(ax::mojom::State::kExpanded);
  } else {
    node_data->AddState(ax::mojom::State::kCollapsed);
  }

  node_data->SetName(accessible_name_);
  node_data->SetValue(model_->GetItemAt(selected_index_));
  if (GetEnabled()) {
    node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kOpen);
  }
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kPosInSet,
                             selected_index_);
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kSetSize,
                             model_->GetItemCount());
}

bool Combobox::HandleAccessibleAction(const ui::AXActionData& action_data) {
  // The action handling in View would generate a mouse event and send it to
  // |this|. However, mouse events for Combobox are handled by |arrow_button_|,
  // which is hidden from the a11y tree (so can't expose actions). Rather than
  // forwarding ax::mojom::Action::kDoDefault to View and then forwarding the
  // mouse event it generates to |arrow_button_| to have it forward back to the
  // callback on |this|, just handle the action explicitly here and bypass View.
  if (GetEnabled() && action_data.action == ax::mojom::Action::kDoDefault) {
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
    return true;
  }
  return View::HandleAccessibleAction(action_data);
}

void Combobox::OnComboboxModelChanged(ui::ComboboxModel* model) {
  DCHECK_EQ(model_, model);

  // If the selection is no longer valid (or the model is empty), restore the
  // default index.
  if (selected_index_ >= model_->GetItemCount() ||
      model_->GetItemCount() == 0 ||
      model_->IsItemSeparatorAt(selected_index_)) {
    SetSelectedIndex(model_->GetDefaultIndex());
  }

  OnContentSizeMaybeChanged();
}

const base::RepeatingClosure& Combobox::GetCallback() const {
  return callback_;
}

const std::unique_ptr<ui::ComboboxModel>& Combobox::GetOwnedModel() const {
  return owned_model_;
}

void Combobox::UpdateBorder() {
  std::unique_ptr<FocusableBorder> border(new FocusableBorder());
  if (invalid_)
    border->SetColorId(ui::kColorAlertHighSeverity);
  SetBorder(std::move(border));
}

void Combobox::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void Combobox::PaintIconAndText(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();
  insets += gfx::Insets(0, LayoutProvider::Get()->GetDistanceMetric(
                               DISTANCE_TEXTFIELD_HORIZONTAL_TEXT_PADDING));

  gfx::ScopedCanvas scoped_canvas(canvas);
  canvas->ClipRect(GetContentsBounds());

  int x = insets.left();
  int y = insets.top();
  int contents_height = height() - insets.height();

  // Draw the icon.
  ui::ImageModel icon = GetModel()->GetIconAt(selected_index_);
  if (!icon.IsEmpty()) {
    gfx::ImageSkia icon_skia =
        GetImageSkiaFromImageModel(icon, GetColorProvider());
    int icon_y = y + (contents_height - icon_skia.height()) / 2;
    gfx::Rect icon_bounds(x, icon_y, icon_skia.width(), icon_skia.height());
    AdjustBoundsForRTLUI(&icon_bounds);
    canvas->DrawImageInt(icon_skia, icon_bounds.x(), icon_bounds.y());
    x += icon_skia.width() + LayoutProvider::Get()->GetDistanceMetric(
                                 DISTANCE_RELATED_LABEL_HORIZONTAL);
  }

  // Draw the text.
  SkColor text_color = GetTextColorForEnableState(*this, GetEnabled());
  // TODO(pbos): Do we need to handle selected_index_ == -1 for unselected here?
  // Ditto on handling an empty model?
  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, GetModel()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ >= GetModel()->GetItemCount())
    SetSelectedIndex(0);

  std::u16string text = GetModel()->GetItemAt(selected_index_);

  int disclosure_arrow_offset = width() - kComboboxArrowContainerWidth;

  const gfx::FontList& font_list = GetFontList();
  int text_width = gfx::GetStringWidth(text, font_list);
  text_width =
      std::min(text_width, disclosure_arrow_offset - insets.right() - x);

  gfx::Rect text_bounds(x, y, text_width, contents_height);
  AdjustBoundsForRTLUI(&text_bounds);
  canvas->DrawStringRect(text, font_list, text_color, text_bounds);

  gfx::Rect arrow_bounds(disclosure_arrow_offset, 0,
                         kComboboxArrowContainerWidth, height());
  arrow_bounds.ClampToCenteredSize(ComboboxArrowSize());
  AdjustBoundsForRTLUI(&arrow_bounds);

  PaintComboboxArrow(text_color, arrow_bounds, canvas);
}

void Combobox::ArrowButtonPressed(const ui::Event& event) {
  if (!GetEnabled())
    return;

  // TODO(hajimehoshi): Fix the problem that the arrow button blinks when
  // cliking this while the dropdown menu is opened.
  if ((base::TimeTicks::Now() - closed_time_) > kMinimumTimeBetweenButtonClicks)
    ShowDropDownMenu(ui::GetMenuSourceTypeForEvent(event));
}

void Combobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  constexpr int kMenuBorderWidthTop = 1;
  // Menu's requested position's width should be the same as local bounds so the
  // border of the menu lines up with the border of the combobox. The y
  // coordinate however should be shifted to the bottom with the border with not
  // to overlap with the combobox border.
  gfx::Rect lb = GetLocalBounds();
  gfx::Point menu_position(lb.origin());
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);

  View::ConvertPointToScreen(this, &menu_position);

  gfx::Rect bounds(menu_position, lb.size());

  Button::ButtonState original_state = arrow_button_->GetState();
  arrow_button_->SetState(Button::STATE_PRESSED);

  // Allow |menu_runner_| to be set by the testing API, but if this method is
  // ever invoked recursively, ensure the old menu is closed.
  if (!menu_runner_ || IsMenuRunning()) {
    menu_runner_ = std::make_unique<MenuRunner>(
        menu_model_.get(), MenuRunner::COMBOBOX,
        base::BindRepeating(&Combobox::OnMenuClosed, base::Unretained(this),
                            original_state));
  }
  menu_runner_->RunMenuAt(GetWidget(), nullptr, bounds,
                          MenuAnchorPosition::kTopLeft, source_type);
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
}

void Combobox::OnMenuClosed(Button::ButtonState original_button_state) {
  menu_runner_.reset();
  arrow_button_->SetState(original_button_state);
  closed_time_ = base::TimeTicks::Now();
  NotifyAccessibilityEvent(ax::mojom::Event::kExpandedChanged, true);
}

void Combobox::OnPerformAction() {
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  SchedulePaint();

  if (callback_)
    callback_.Run();

  // Note |this| may be deleted by |callback_|.
}

gfx::Size Combobox::GetContentSize() const {
  const gfx::FontList& font_list = GetFontList();
  int height = font_list.GetHeight();
  int width = 0;
  for (int i = 0; i < GetModel()->GetItemCount(); ++i) {
    if (model_->IsItemSeparatorAt(i))
      continue;

    if (size_to_largest_label_ || i == selected_index_) {
      int item_width = gfx::GetStringWidth(GetModel()->GetItemAt(i), font_list);
      ui::ImageModel icon = GetModel()->GetIconAt(i);
      if (!icon.IsEmpty()) {
        gfx::ImageSkia icon_skia;
        if (GetWidget())
          icon_skia = GetImageSkiaFromImageModel(icon, GetColorProvider());
        item_width +=
            icon_skia.width() + LayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_RELATED_LABEL_HORIZONTAL);
        height = std::max(height, icon_skia.height());
      }
      width = std::max(width, item_width);
    }
  }
  return gfx::Size(width, height);
}

void Combobox::OnContentSizeMaybeChanged() {
  content_size_ = GetContentSize();
  PreferredSizeChanged();
}

PrefixSelector* Combobox::GetPrefixSelector() {
  if (!selector_)
    selector_ = std::make_unique<PrefixSelector>(this, this);
  return selector_.get();
}

BEGIN_METADATA(Combobox, View)
ADD_PROPERTY_METADATA(base::RepeatingClosure, Callback)
ADD_PROPERTY_METADATA(std::unique_ptr<ui::ComboboxModel>, OwnedModel)
ADD_PROPERTY_METADATA(ui::ComboboxModel*, Model)
ADD_PROPERTY_METADATA(int, SelectedIndex)
ADD_PROPERTY_METADATA(bool, Invalid)
ADD_PROPERTY_METADATA(bool, SizeToLargestLabel)
ADD_PROPERTY_METADATA(std::u16string, AccessibleName)
ADD_PROPERTY_METADATA(std::u16string, TooltipTextAndAccessibleName)
END_METADATA

}  // namespace views
