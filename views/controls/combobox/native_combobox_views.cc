// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/combobox/native_combobox_views.h"

#include <algorithm>

#include "base/command_line.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/path.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/focusable_border.h"
#include "views/controls/menu/menu_2.h"
#include "views/widget/root_view.h"

#if defined(OS_LINUX)
#include "ui/gfx/gtk_util.h"
#endif

namespace {

// A global flag to switch the Combobox wrapper to NativeComboboxViews.
bool combobox_view_enabled = false;

// Define the size of the insets
const int kTopInsetSize = 4;
const int kLeftInsetSize = 4;
const int kBottomInsetSize = 4;
const int kRightInsetSize = 4;

// Limit how small a combobox can be.
const int kMinComboboxWidth = 148;

// Size of the combobox arrow
const int kComboboxArrowSize = 9;
const int kComboboxArrowOffset = 7;
const int kComboboxArrowMargin = 12;

// Color settings for text and border.
// These are tentative, and should be derived from theme, system
// settings and current settings.
const SkColor kDefaultBorderColor = SK_ColorGRAY;
const SkColor kTextColor = SK_ColorBLACK;

}  // namespace

namespace views {

const char NativeComboboxViews::kViewClassName[] =
    "views/NativeComboboxViews";

NativeComboboxViews::NativeComboboxViews(Combobox* parent)
    : combobox_(parent),
      text_border_(new FocusableBorder()),
      dropdown_open_(false),
      selected_item_(-1),
      content_width_(0),
      content_height_(0) {
  set_border(text_border_);
}

NativeComboboxViews::~NativeComboboxViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeComboboxViews, View overrides:

bool NativeComboboxViews::OnMousePressed(const views::MouseEvent& e) {
  combobox_->RequestFocus();
  if (e.IsLeftMouseButton()) {
    UpdateFromModel();
    ShowDropDownMenu();
  }

  return true;
}

bool NativeComboboxViews::OnMouseDragged(const views::MouseEvent& event) {
  return true;
}

bool NativeComboboxViews::OnKeyPressed(const views::KeyEvent& event) {
  // OnKeyPressed/OnKeyReleased/OnFocus/OnBlur will never be invoked on
  // NativeComboboxViews as it will never gain focus.
  NOTREACHED();
  return false;
}

bool NativeComboboxViews::OnKeyReleased(const views::KeyEvent& event) {
  NOTREACHED();
  return false;
}

void NativeComboboxViews::OnPaint(gfx::Canvas* canvas) {
  text_border_->set_has_focus(combobox_->HasFocus());
  OnPaintBackground(canvas);
  PaintText(canvas);
  OnPaintBorder(canvas);
}

void NativeComboboxViews::OnFocus() {
  NOTREACHED();
}

void NativeComboboxViews::OnBlur() {
  NOTREACHED();
}

/////////////////////////////////////////////////////////////////
// NativeComboboxViews, NativeComboboxWrapper overrides:

void NativeComboboxViews::UpdateFromModel() {
  int max_width = 0;
  const gfx::Font &font = GetFont();

  // retrieve the items from the model and add them to our own menu
  dropdown_list_model_.reset(new ui::SimpleMenuModel(this));
  int num_items = combobox_->model()->GetItemCount();
  for (int i = 0; i < num_items; ++i) {
    // TODO(saintlou): figure out RTL issues
    string16 text = combobox_->model()->GetItemAt(i);
    dropdown_list_model_->AddItem(i, text);
    max_width = std::max(max_width, font.GetStringWidth(text));
  }

  dropdown_list_menu_.reset(new Menu2(dropdown_list_model_.get()));
  content_width_ = max_width;
  content_height_ = font.GetFontSize();
}

void NativeComboboxViews::UpdateSelectedItem() {
  selected_item_ = combobox_->selected_item();
}

void NativeComboboxViews::UpdateEnabled() {
  SetEnabled(combobox_->IsEnabled());
}

int NativeComboboxViews::GetSelectedItem() const {
  return selected_item_;
}

bool NativeComboboxViews::IsDropdownOpen() const {
  return dropdown_open_;
}

gfx::Size NativeComboboxViews::GetPreferredSize() {
  if (content_width_ == 0)
    UpdateFromModel();

  // TODO(saintlou) the preferred size will drive the local bounds
  // which in turn is used to set the minimum width for the dropdown
  gfx::Insets insets = GetInsets();
  return gfx::Size(std::min(kMinComboboxWidth,
                            content_width_ + 2 * (insets.width())),
                   content_height_ + 2 * (insets.height()));
}

View* NativeComboboxViews::GetView() {
  return this;
}

void NativeComboboxViews::SetFocus() {
  text_border_->set_has_focus(true);
}

gfx::NativeView NativeComboboxViews::GetTestingHandle() const {
  NOTREACHED();
  return NULL;
}

/////////////////////////////////////////////////////////////////
// NativeComboboxViews, ui::SimpleMenuModel::Delegate overrides:

bool NativeComboboxViews::IsCommandIdChecked(int command_id) const {
  return true;
}

bool NativeComboboxViews::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool NativeComboboxViews::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void NativeComboboxViews::ExecuteCommand(int command_id) {
  if (command_id >= 0 && command_id < combobox_->model()->GetItemCount()) {
    selected_item_ = command_id;
    combobox_->SelectionChanged();
  }
  else
    NOTREACHED() << "unknown command: " << command_id;
}


// static
bool NativeComboboxViews::IsComboboxViewsEnabled() {
#if defined(TOUCH_UI)
  return true;
#else
  return combobox_view_enabled || RootView::IsPureViews();
#endif
}

// static
void NativeComboboxViews::SetEnableComboboxViews(bool enabled) {
  combobox_view_enabled = enabled;
}

/////////////////////////////////////////////////////////////////
// NativeComboboxViews private methods:

const gfx::Font& NativeComboboxViews::GetFont() const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
 #if 0 // For now the width calculation is off so stick with !defined(TOUCH_UI)
  return rb.GetFont(ResourceBundle::LargeFont);
 #else
  return rb.GetFont(ResourceBundle::BaseFont);
 #endif
}

// tip_x and tip_y are the coordinates of the tip of an arrow head which is
// drawn as an isoscele triangle
// shift_x and shift_y are offset from tip_x and tip_y to specify the relative
// positions of the 2 equal angles (ie not the angle at the tip of the arrow)
// Note: the width of the base (the side opposite the tip) is 2 * shift_x
void NativeComboboxViews::DrawArrow(gfx::Canvas* canvas,
                                     int tip_x,
                                     int tip_y,
                                     int shift_x,
                                     int shift_y) const {
  SkPaint paint;
  paint.setStyle(SkPaint::kStrokeAndFill_Style);
  paint.setColor(kTextColor);
  paint.setAntiAlias(true);
  gfx::Path path;
  path.incReserve(4);
  path.moveTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
  path.lineTo(SkIntToScalar(tip_x + shift_x), SkIntToScalar(tip_y + shift_y));
  path.lineTo(SkIntToScalar(tip_x - shift_x), SkIntToScalar(tip_y + shift_y));
  path.close();
  canvas->AsCanvasSkia()->drawPath(path, paint);
}


void NativeComboboxViews::PaintText(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  canvas->Save();
  canvas->ClipRectInt(insets.left(), insets.top(),
                      width() - insets.width(), height() - insets.height());

  int x = insets.left();
  int y = insets.top();
  int text_height = height() - insets.height();
  SkColor text_color = kTextColor;

  int index = GetSelectedItem();
  if (index < 0 || index > combobox_->model()->GetItemCount())
    index = 0;
  string16 text = combobox_->model()->GetItemAt(index);

  const gfx::Font &font = GetFont();
  int width = font.GetStringWidth(text);

  canvas->DrawStringInt(text, font, text_color, x, y, width, text_height);

  // draw the double arrow
  gfx::Rect lb = GetLocalBounds();
  DrawArrow(canvas,
      lb.width() - (kComboboxArrowSize / 2) - kComboboxArrowOffset,
      lb.height() / 2 - kComboboxArrowSize,
      kComboboxArrowSize / 2,
      kComboboxArrowSize - 2);
  DrawArrow(canvas,
      lb.width() - (kComboboxArrowSize / 2) - kComboboxArrowOffset,
      lb.height() / 2 + kComboboxArrowSize,
      -kComboboxArrowSize / 2,
      -(kComboboxArrowSize - 2));

  // draw the margin
  canvas->DrawLineInt(kDefaultBorderColor,
      lb.width() - kComboboxArrowSize - kComboboxArrowMargin,
      kTopInsetSize,
      lb.width() - kComboboxArrowSize - kComboboxArrowMargin,
      lb.height() - kBottomInsetSize);

  canvas->Restore();
}

void NativeComboboxViews::ShowDropDownMenu() {
  if (dropdown_list_model_.get()) {
    gfx::Rect lb = GetLocalBounds();

    // Both the menu position and the menu anchor type change if the UI layout
    // is right-to-left.
    gfx::Point menu_position(lb.origin());

    if (base::i18n::IsRTL())
      menu_position.Offset(lb.width() - 1, 0);

    View::ConvertPointToScreen(this, &menu_position);

    if (menu_position.x() < 0)
      menu_position.set_x(0);

    // Open the menu
    dropdown_list_menu_.reset(new Menu2(dropdown_list_model_.get()));
    dropdown_list_menu_->SetMinimumWidth(lb.width());
    dropdown_open_ = true;
    dropdown_list_menu_->RunMenuAt(menu_position, Menu2::ALIGN_TOPLEFT);
    dropdown_open_ = false;

    // Need to explicitly clear mouse handler so that events get sent
    // properly after the menu finishes running. If we don't do this, then
    // the first click to other parts of the UI is eaten.
    SetMouseHandler(NULL);
  }
}

}  // namespace views
