// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/color_constants.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/prefix_selector.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// Menu border widths
const int kMenuBorderWidthLeft = 1;
const int kMenuBorderWidthTop = 1;
const int kMenuBorderWidthRight = 1;

// Limit how small a combobox can be.
const int kMinComboboxWidth = 25;

// Size of the combobox arrow margins
const int kDisclosureArrowLeftPadding = 7;
const int kDisclosureArrowRightPadding = 7;

// Define the id of the first item in the menu (since it needs to be > 0)
const int kFirstMenuItemId = 1000;

const SkColor kInvalidTextColor = SK_ColorWHITE;

// Used to indicate that no item is currently selected by the user.
const int kNoSelection = -1;

// The background to use for invalid comboboxes.
class InvalidBackground : public Background {
 public:
  InvalidBackground() {}
  virtual ~InvalidBackground() {}

  // Overridden from Background:
  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE {
    gfx::Rect bounds(view->GetLocalBounds());
    // Inset by 2 to leave 1 empty pixel between background and border.
    bounds.Inset(2, 2, 2, 2);
    canvas->FillRect(bounds, kWarningColor);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InvalidBackground);
};

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

}  // namespace

// static
const char Combobox::kViewClassName[] = "views/Combobox";

////////////////////////////////////////////////////////////////////////////////
// Combobox, public:

Combobox::Combobox(ui::ComboboxModel* model)
    : model_(model),
      listener_(NULL),
      selected_index_(model_->GetDefaultIndex()),
      invalid_(false),
      text_border_(new FocusableBorder()),
      disclosure_arrow_(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_MENU_DROPARROW).ToImageSkia()),
      dropdown_open_(false) {
  model_->AddObserver(this);
  UpdateFromModel();
  set_focusable(true);
  set_border(text_border_);
}

Combobox::~Combobox() {
  model_->RemoveObserver(this);
}

// static
const gfx::Font& Combobox::GetFont() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFont(ui::ResourceBundle::BaseFont);
}

void Combobox::ModelChanged() {
  selected_index_ = std::min(0, model_->GetItemCount());
  UpdateFromModel();
  PreferredSizeChanged();
}

void Combobox::SetSelectedIndex(int index) {
  selected_index_ = index;
  SchedulePaint();
}

bool Combobox::SelectValue(const base::string16& value) {
  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (value == model()->GetItemAt(i)) {
      SetSelectedIndex(i);
      return true;
    }
  }
  return false;
}

void Combobox::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

void Combobox::SetInvalid(bool invalid) {
  invalid_ = invalid;
  if (invalid) {
    text_border_->SetColor(kWarningColor);
    set_background(new InvalidBackground());
  } else {
    text_border_->UseDefaultColor();
    set_background(NULL);
  }
}

ui::TextInputClient* Combobox::GetTextInputClient() {
  if (!selector_)
    selector_.reset(new PrefixSelector(this));
  return selector_.get();
}


bool Combobox::IsItemChecked(int id) const {
  return false;
}

bool Combobox::IsCommandEnabled(int id) const {
  return model()->IsItemEnabledAt(MenuCommandToIndex(id));
}

void Combobox::ExecuteCommand(int id) {
  selected_index_ = MenuCommandToIndex(id);
  OnSelectionChanged();
}

bool Combobox::GetAccelerator(int id, ui::Accelerator* accel) {
  return false;
}

int Combobox::GetRowCount() {
  return model()->GetItemCount();
}

int Combobox::GetSelectedRow() {
  return selected_index_;
}

void Combobox::SetSelectedRow(int row) {
  SetSelectedIndex(row);
}

string16 Combobox::GetTextForRow(int row) {
  return model()->IsItemSeparatorAt(row) ? string16() : model()->GetItemAt(row);
}

////////////////////////////////////////////////////////////////////////////////
// Combobox, View overrides:

gfx::Size Combobox::GetPreferredSize() {
  if (content_size_.IsEmpty())
    UpdateFromModel();

  // The preferred size will drive the local bounds which in turn is used to set
  // the minimum width for the dropdown list.
  gfx::Insets insets = GetInsets();
  int total_width = std::max(kMinComboboxWidth, content_size_.width()) +
      insets.width() + kDisclosureArrowLeftPadding +
      disclosure_arrow_->width() + kDisclosureArrowRightPadding;

  return gfx::Size(total_width, content_size_.height() + insets.height());
}

const char* Combobox::GetClassName() const {
  return kViewClassName;
}

bool Combobox::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Escape should close the drop down list when it is active, not host UI.
  if (e.key_code() != ui::VKEY_ESCAPE ||
      e.IsShiftDown() || e.IsControlDown() || e.IsAltDown()) {
    return false;
  }
  return dropdown_open_;
}

bool Combobox::OnMousePressed(const ui::MouseEvent& mouse_event) {
  RequestFocus();
  const base::TimeDelta delta = base::Time::Now() - closed_time_;
  if (mouse_event.IsLeftMouseButton() &&
      (delta.InMilliseconds() > kMinimumMsBetweenButtonClicks)) {
    UpdateFromModel();
    ShowDropDownMenu(ui::MENU_SOURCE_MOUSE);
  }

  return true;
}

bool Combobox::OnMouseDragged(const ui::MouseEvent& mouse_event) {
  return true;
}

bool Combobox::OnKeyPressed(const ui::KeyEvent& e) {
  // TODO(oshima): handle IME.
  DCHECK_EQ(e.type(), ui::ET_KEY_PRESSED);

  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;

  bool show_menu = false;
  int new_index = kNoSelection;
  switch (e.key_code()) {
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
        new_index = GetAdjacentIndex(model(), 1, selected_index_);
      break;

    // Move to the end of the list.
    case ui::VKEY_END:
    case ui::VKEY_NEXT:  // Page down.
      new_index = GetAdjacentIndex(model(), -1, model()->GetItemCount());
      break;

    // Move to the beginning of the list.
    case ui::VKEY_HOME:
    case ui::VKEY_PRIOR:  // Page up.
      new_index = GetAdjacentIndex(model(), 1, -1);
      break;

    // Move to the previous item if any.
    case ui::VKEY_UP:
      new_index = GetAdjacentIndex(model(), -1, selected_index_);
      break;

    default:
      return false;
  }

  if (show_menu) {
    UpdateFromModel();
    ShowDropDownMenu(ui::MENU_SOURCE_KEYBOARD);
  } else if (new_index != selected_index_ && new_index != kNoSelection) {
    DCHECK(!model()->IsItemSeparatorAt(new_index));
    selected_index_ = new_index;
    OnSelectionChanged();
  }

  return true;
}

bool Combobox::OnKeyReleased(const ui::KeyEvent& e) {
  return false;  // crbug.com/127520
}

void Combobox::OnGestureEvent(ui::GestureEvent* gesture) {
  if (gesture->type() == ui::ET_GESTURE_TAP) {
    UpdateFromModel();
    ShowDropDownMenu(ui::MENU_SOURCE_TOUCH);
    gesture->StopPropagation();
    return;
  }
  View::OnGestureEvent(gesture);
}

void Combobox::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintText(canvas);
  OnPaintBorder(canvas);
}

void Combobox::OnFocus() {
  GetInputMethod()->OnFocus();
  text_border_->set_has_focus(true);
  View::OnFocus();
}

void Combobox::OnBlur() {
  GetInputMethod()->OnBlur();
  if (selector_)
    selector_->OnViewBlur();
  text_border_->set_has_focus(false);
}

void Combobox::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_COMBOBOX;
  state->name = accessible_name_;
  state->value = model_->GetItemAt(selected_index_);
  state->index = selected_index_;
  state->count = model_->GetItemCount();
}

void Combobox::OnModelChanged() {
  ModelChanged();
}

void Combobox::UpdateFromModel() {
  int max_width = 0;
  const gfx::Font& font = Combobox::GetFont();

  MenuItemView* menu = new MenuItemView(this);
  // MenuRunner owns |menu|.
  dropdown_list_menu_runner_.reset(new MenuRunner(menu));

  int num_items = model()->GetItemCount();
  for (int i = 0; i < num_items; ++i) {
    if (model()->IsItemSeparatorAt(i)) {
      menu->AppendSeparator();
      continue;
    }

    string16 text = model()->GetItemAt(i);

    // Inserting the Unicode formatting characters if necessary so that the
    // text is displayed correctly in right-to-left UIs.
    base::i18n::AdjustStringForLocaleDirection(&text);

    menu->AppendMenuItem(i + kFirstMenuItemId, text, MenuItemView::NORMAL);
    max_width = std::max(max_width, font.GetStringWidth(text));
  }

  content_size_.SetSize(max_width, font.GetHeight());
}

void Combobox::AdjustBoundsForRTLUI(gfx::Rect* rect) const {
  rect->set_x(GetMirroredXForRect(*rect));
}

void Combobox::PaintText(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  canvas->Save();
  canvas->ClipRect(GetContentsBounds());

  int x = insets.left();
  int y = insets.top();
  int text_height = height() - insets.height();
  SkColor text_color = invalid() ? kInvalidTextColor :
      GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LabelEnabledColor);

  DCHECK_GE(selected_index_, 0);
  DCHECK_LT(selected_index_, model()->GetItemCount());
  if (selected_index_ < 0 || selected_index_ > model()->GetItemCount())
    selected_index_ = 0;
  string16 text = model()->GetItemAt(selected_index_);

  int disclosure_arrow_offset = width() - disclosure_arrow_->width()
      - kDisclosureArrowLeftPadding - kDisclosureArrowRightPadding;

  const gfx::Font& font = Combobox::GetFont();
  int text_width = font.GetStringWidth(text);
  if ((text_width + insets.width()) > disclosure_arrow_offset)
    text_width = disclosure_arrow_offset - insets.width();

  gfx::Rect text_bounds(x, y, text_width, text_height);
  AdjustBoundsForRTLUI(&text_bounds);
  canvas->DrawStringInt(text, font, text_color, text_bounds);

  gfx::Rect arrow_bounds(disclosure_arrow_offset + kDisclosureArrowLeftPadding,
                         height() / 2 - disclosure_arrow_->height() / 2,
                         disclosure_arrow_->width(),
                         disclosure_arrow_->height());
  AdjustBoundsForRTLUI(&arrow_bounds);

  SkPaint paint;
  // This makes the arrow subtractive.
  if (invalid())
    paint.setXfermodeMode(SkXfermode::kDstOut_Mode);
  canvas->DrawImageInt(*disclosure_arrow_, arrow_bounds.x(), arrow_bounds.y(),
                       paint);

  canvas->Restore();
}

void Combobox::ShowDropDownMenu(ui::MenuSourceType source_type) {
  if (!dropdown_list_menu_runner_.get())
    UpdateFromModel();

  // Extend the menu to the width of the combobox.
  MenuItemView* menu = dropdown_list_menu_runner_->GetMenu();
  SubmenuView* submenu = menu->CreateSubmenu();
  submenu->set_minimum_preferred_width(size().width() -
                                (kMenuBorderWidthLeft + kMenuBorderWidthRight));

  gfx::Rect lb = GetLocalBounds();
  gfx::Point menu_position(lb.origin());

  // Inset the menu's requested position so the border of the menu lines up
  // with the border of the combobox.
  menu_position.set_x(menu_position.x() + kMenuBorderWidthLeft);
  menu_position.set_y(menu_position.y() + kMenuBorderWidthTop);
  lb.set_width(lb.width() - (kMenuBorderWidthLeft + kMenuBorderWidthRight));

  View::ConvertPointToScreen(this, &menu_position);
  if (menu_position.x() < 0)
      menu_position.set_x(0);

  gfx::Rect bounds(menu_position, lb.size());

  dropdown_open_ = true;
  if (dropdown_list_menu_runner_->RunMenuAt(GetWidget(), NULL, bounds,
          MenuItemView::TOPLEFT, source_type, MenuRunner::COMBOBOX) ==
      MenuRunner::MENU_DELETED)
    return;
  dropdown_open_ = false;
  closed_time_ = base::Time::Now();

  // Need to explicitly clear mouse handler so that events get sent
  // properly after the menu finishes running. If we don't do this, then
  // the first click to other parts of the UI is eaten.
  SetMouseHandler(NULL);
}

void Combobox::OnSelectionChanged() {
  if (listener_)
    listener_->OnSelectedIndexChanged(this);
  NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_VALUE_CHANGED, false);
  SchedulePaint();
}

int Combobox::MenuCommandToIndex(int menu_command_id) const {
  // (note that the id received is offset by kFirstMenuItemId)
  // Revert menu ID offset to map back to combobox model.
  int index = menu_command_id - kFirstMenuItemId;
  DCHECK_LT(index, model()->GetItemCount());
  return index;
}

}  // namespace views
