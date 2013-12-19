// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <string>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/selection_model.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/textfield/native_textfield_views.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/painter.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

namespace {

// Default placeholder text color.
const SkColor kDefaultPlaceholderTextColor = SK_ColorLTGRAY;

gfx::FontList GetDefaultFontList() {
  return ResourceBundle::GetSharedInstance().GetFontList(
      ResourceBundle::BaseFont);
}

}  // namespace

namespace views {

// static
const char Textfield::kViewClassName[] = "Textfield";

// static
size_t Textfield::GetCaretBlinkMs() {
  static const size_t default_value = 500;
#if defined(OS_WIN)
  static const size_t system_value = ::GetCaretBlinkTime();
  if (system_value != 0)
    return (system_value == INFINITE) ? 0 : system_value;
#endif
  return default_value;
}

Textfield::Textfield()
    : textfield_view_(NULL),
      controller_(NULL),
      style_(STYLE_DEFAULT),
      font_list_(GetDefaultFontList()),
      read_only_(false),
      default_width_in_chars_(0),
      draw_border_(true),
      text_color_(SK_ColorBLACK),
      use_default_text_color_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      horizontal_margins_were_set_(false),
      vertical_margins_were_set_(false),
      placeholder_text_color_(kDefaultPlaceholderTextColor),
      text_input_type_(ui::TEXT_INPUT_TYPE_TEXT),
      weak_ptr_factory_(this) {
  SetFocusable(true);

  if (ViewsDelegate::views_delegate) {
    obscured_reveal_duration_ = ViewsDelegate::views_delegate->
        GetDefaultTextfieldObscuredRevealDuration();
  }

  if (NativeViewHost::kRenderNativeControlFocus)
    focus_painter_ = Painter::CreateDashedFocusPainter();
}

Textfield::Textfield(StyleFlags style)
    : textfield_view_(NULL),
      controller_(NULL),
      style_(style),
      font_list_(GetDefaultFontList()),
      read_only_(false),
      default_width_in_chars_(0),
      draw_border_(true),
      text_color_(SK_ColorBLACK),
      use_default_text_color_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      horizontal_margins_were_set_(false),
      vertical_margins_were_set_(false),
      placeholder_text_color_(kDefaultPlaceholderTextColor),
      text_input_type_(ui::TEXT_INPUT_TYPE_TEXT),
      weak_ptr_factory_(this) {
  SetFocusable(true);
  if (IsObscured())
    SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  if (ViewsDelegate::views_delegate) {
    obscured_reveal_duration_ = ViewsDelegate::views_delegate->
        GetDefaultTextfieldObscuredRevealDuration();
  }

  if (NativeViewHost::kRenderNativeControlFocus)
    focus_painter_ = Painter::CreateDashedFocusPainter();
}

Textfield::~Textfield() {
}

void Textfield::SetController(TextfieldController* controller) {
  controller_ = controller;
}

TextfieldController* Textfield::GetController() const {
  return controller_;
}

void Textfield::SetReadOnly(bool read_only) {
  // Update read-only without changing the focusable state (or active, etc.).
  read_only_ = read_only;
  if (textfield_view_) {
    textfield_view_->UpdateReadOnly();
    textfield_view_->UpdateTextColor();
    textfield_view_->UpdateBackgroundColor();
  }
}

bool Textfield::IsObscured() const {
  return style_ & STYLE_OBSCURED;
}

void Textfield::SetObscured(bool obscured) {
  if (obscured) {
    style_ = static_cast<StyleFlags>(style_ | STYLE_OBSCURED);
    SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  } else {
    style_ = static_cast<StyleFlags>(style_ & ~STYLE_OBSCURED);
    SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
  }
  if (textfield_view_)
    textfield_view_->UpdateIsObscured();
}

ui::TextInputType Textfield::GetTextInputType() const {
  if (read_only() || !enabled())
    return ui::TEXT_INPUT_TYPE_NONE;
  return text_input_type_;
}

void Textfield::SetTextInputType(ui::TextInputType type) {
  text_input_type_ = type;
  bool should_be_obscured = type == ui::TEXT_INPUT_TYPE_PASSWORD;
  if (IsObscured() != should_be_obscured)
    SetObscured(should_be_obscured);
}

void Textfield::SetText(const base::string16& text) {
  text_ = text;
  if (textfield_view_)
    textfield_view_->UpdateText();
}

void Textfield::AppendText(const base::string16& text) {
  text_ += text;
  if (textfield_view_)
    textfield_view_->AppendText(text);
}

void Textfield::InsertOrReplaceText(const base::string16& text) {
  if (textfield_view_) {
    textfield_view_->InsertOrReplaceText(text);
    text_ = textfield_view_->GetText();
  }
}

base::i18n::TextDirection Textfield::GetTextDirection() const {
  return textfield_view_ ?
      textfield_view_->GetTextDirection() : base::i18n::UNKNOWN_DIRECTION;
}

void Textfield::SelectAll(bool reversed) {
  if (textfield_view_)
    textfield_view_->SelectAll(reversed);
}

base::string16 Textfield::GetSelectedText() const {
  return textfield_view_ ? textfield_view_->GetSelectedText() :
                           base::string16();
}

void Textfield::ClearSelection() const {
  if (textfield_view_)
    textfield_view_->ClearSelection();
}

bool Textfield::HasSelection() const {
  return textfield_view_ && !textfield_view_->GetSelectedRange().is_empty();
}

SkColor Textfield::GetTextColor() const {
  if (!use_default_text_color_)
    return text_color_;

  return GetNativeTheme()->GetSystemColor(read_only() ?
      ui::NativeTheme::kColorId_TextfieldReadOnlyColor :
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

void Textfield::SetTextColor(SkColor color) {
  text_color_ = color;
  use_default_text_color_ = false;
  if (textfield_view_)
    textfield_view_->UpdateTextColor();
}

void Textfield::UseDefaultTextColor() {
  use_default_text_color_ = true;
  if (textfield_view_)
    textfield_view_->UpdateTextColor();
}

SkColor Textfield::GetBackgroundColor() const {
  if (!use_default_background_color_)
    return background_color_;

  return GetNativeTheme()->GetSystemColor(read_only() ?
      ui::NativeTheme::kColorId_TextfieldReadOnlyBackground :
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
}

void Textfield::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  use_default_background_color_ = false;
  if (textfield_view_)
    textfield_view_->UpdateBackgroundColor();
}

void Textfield::UseDefaultBackgroundColor() {
  use_default_background_color_ = true;
  if (textfield_view_)
    textfield_view_->UpdateBackgroundColor();
}

bool Textfield::GetCursorEnabled() const {
  return textfield_view_ && textfield_view_->GetCursorEnabled();
}

void Textfield::SetCursorEnabled(bool enabled) {
  if (textfield_view_)
    textfield_view_->SetCursorEnabled(enabled);
}

void Textfield::SetFontList(const gfx::FontList& font_list) {
  font_list_ = font_list;
  if (textfield_view_)
    textfield_view_->UpdateFont();
  PreferredSizeChanged();
}

const gfx::Font& Textfield::GetPrimaryFont() const {
  return font_list_.GetPrimaryFont();
}

void Textfield::SetFont(const gfx::Font& font) {
  SetFontList(gfx::FontList(font));
}

void Textfield::SetHorizontalMargins(int left, int right) {
  if (horizontal_margins_were_set_ &&
      left == margins_.left() && right == margins_.right()) {
    return;
  }
  margins_.Set(margins_.top(), left, margins_.bottom(), right);
  horizontal_margins_were_set_ = true;
  if (textfield_view_)
    textfield_view_->UpdateHorizontalMargins();
  PreferredSizeChanged();
}

void Textfield::SetVerticalMargins(int top, int bottom) {
  if (vertical_margins_were_set_ &&
      top == margins_.top() && bottom == margins_.bottom()) {
    return;
  }
  margins_.Set(top, margins_.left(), bottom, margins_.right());
  vertical_margins_were_set_ = true;
  if (textfield_view_)
    textfield_view_->UpdateVerticalMargins();
  PreferredSizeChanged();
}

void Textfield::RemoveBorder() {
  if (!draw_border_)
    return;

  draw_border_ = false;
  if (textfield_view_)
    textfield_view_->UpdateBorder();
}

base::string16 Textfield::GetPlaceholderText() const {
  return placeholder_text_;
}

bool Textfield::GetHorizontalMargins(int* left, int* right) {
  if (!horizontal_margins_were_set_)
    return false;

  *left = margins_.left();
  *right = margins_.right();
  return true;
}

bool Textfield::GetVerticalMargins(int* top, int* bottom) {
  if (!vertical_margins_were_set_)
    return false;

  *top = margins_.top();
  *bottom = margins_.bottom();
  return true;
}

void Textfield::UpdateAllProperties() {
  if (textfield_view_) {
    textfield_view_->UpdateText();
    textfield_view_->UpdateTextColor();
    textfield_view_->UpdateBackgroundColor();
    textfield_view_->UpdateReadOnly();
    textfield_view_->UpdateFont();
    textfield_view_->UpdateEnabled();
    textfield_view_->UpdateBorder();
    textfield_view_->UpdateIsObscured();
    textfield_view_->UpdateHorizontalMargins();
    textfield_view_->UpdateVerticalMargins();
  }
}

void Textfield::SyncText() {
  if (textfield_view_) {
    base::string16 new_text = textfield_view_->GetText();
    if (new_text != text_) {
      text_ = new_text;
      if (controller_)
        controller_->ContentsChanged(this, text_);
    }
  }
}

bool Textfield::IsIMEComposing() const {
  return textfield_view_ && textfield_view_->IsIMEComposing();
}

const gfx::Range& Textfield::GetSelectedRange() const {
  return textfield_view_->GetSelectedRange();
}

void Textfield::SelectRange(const gfx::Range& range) {
  textfield_view_->SelectRange(range);
}

const gfx::SelectionModel& Textfield::GetSelectionModel() const {
  return textfield_view_->GetSelectionModel();
}

void Textfield::SelectSelectionModel(const gfx::SelectionModel& sel) {
  textfield_view_->SelectSelectionModel(sel);
}

size_t Textfield::GetCursorPosition() const {
  return textfield_view_->GetCursorPosition();
}

void Textfield::SetColor(SkColor value) {
  return textfield_view_->SetColor(value);
}

void Textfield::ApplyColor(SkColor value, const gfx::Range& range) {
  return textfield_view_->ApplyColor(value, range);
}

void Textfield::SetStyle(gfx::TextStyle style, bool value) {
  return textfield_view_->SetStyle(style, value);
}

void Textfield::ApplyStyle(gfx::TextStyle style,
                           bool value,
                           const gfx::Range& range) {
  return textfield_view_->ApplyStyle(style, value, range);
}

void Textfield::ClearEditHistory() {
  textfield_view_->ClearEditHistory();
}

void Textfield::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

void Textfield::ExecuteCommand(int command_id) {
  textfield_view_->ExecuteCommand(command_id, ui::EF_NONE);
}

void Textfield::SetFocusPainter(scoped_ptr<Painter> focus_painter) {
  focus_painter_ = focus_painter.Pass();
}

bool Textfield::HasTextBeingDragged() {
  return textfield_view_->HasTextBeingDragged();
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, View overrides:

void Textfield::Layout() {
  if (textfield_view_) {
    textfield_view_->SetBoundsRect(GetContentsBounds());
    textfield_view_->Layout();
  }
}

int Textfield::GetBaseline() const {
  gfx::Insets insets = GetTextInsets();
  const int baseline = textfield_view_ ?
      textfield_view_->GetTextfieldBaseline() : font_list_.GetBaseline();
  return insets.top() + baseline;
}

gfx::Size Textfield::GetPreferredSize() {
  gfx::Insets insets = GetTextInsets();

  const int font_height = textfield_view_ ? textfield_view_->GetFontHeight() :
                                            font_list_.GetHeight();
  return gfx::Size(
      GetPrimaryFont().GetExpectedTextWidth(default_width_in_chars_)
      + insets.width(),
      font_height + insets.height());
}

void Textfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll(false);
}

bool Textfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Skip any accelerator handling of backspace; textfields handle this key.
  // Also skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  return e.key_code() == ui::VKEY_BACK || e.IsUnicodeKeyCode();
}

void Textfield::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (NativeViewHost::kRenderNativeControlFocus)
    Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

bool Textfield::OnKeyPressed(const ui::KeyEvent& e) {
  return textfield_view_ && textfield_view_->HandleKeyPressed(e);
}

bool Textfield::OnKeyReleased(const ui::KeyEvent& e) {
  return textfield_view_ && textfield_view_->HandleKeyReleased(e);
}

bool Textfield::OnMouseDragged(const ui::MouseEvent& e) {
  if (!e.IsOnlyRightMouseButton())
    return View::OnMouseDragged(e);
  return true;
}

void Textfield::OnFocus() {
  if (textfield_view_)
    textfield_view_->HandleFocus();
  View::OnFocus();
  SchedulePaint();
}

void Textfield::OnBlur() {
  if (textfield_view_)
    textfield_view_->HandleBlur();

  // Border typically draws focus indicator.
  SchedulePaint();
}

void Textfield::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TEXT;
  state->name = accessible_name_;
  if (read_only())
    state->state |= ui::AccessibilityTypes::STATE_READONLY;
  if (IsObscured())
    state->state |= ui::AccessibilityTypes::STATE_PROTECTED;
  state->value = text_;

  const gfx::Range range = textfield_view_->GetSelectedRange();
  state->selection_start = range.start();
  state->selection_end = range.end();

  if (!read_only()) {
    state->set_value_callback =
        base::Bind(&Textfield::AccessibilitySetValue,
                   weak_ptr_factory_.GetWeakPtr());
  }
}

ui::TextInputClient* Textfield::GetTextInputClient() {
  return textfield_view_ ? textfield_view_->GetTextInputClient() : NULL;
}

gfx::Point Textfield::GetKeyboardContextMenuLocation() {
  return textfield_view_ ? textfield_view_->GetContextMenuLocation() :
                           View::GetKeyboardContextMenuLocation();
}

void Textfield::OnEnabledChanged() {
  View::OnEnabledChanged();
  if (textfield_view_)
    textfield_view_->UpdateEnabled();
}

void Textfield::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !textfield_view_ && GetWidget()) {
    // The textfield view's lifetime is managed by the view hierarchy.
    textfield_view_ = new NativeTextfieldViews(this);
    AddChildViewAt(textfield_view_, 0);
    Layout();
    UpdateAllProperties();
  }
}

const char* Textfield::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, private:

gfx::Insets Textfield::GetTextInsets() const {
  gfx::Insets insets = GetInsets();
  if (draw_border_ && textfield_view_)
    insets += textfield_view_->GetInsets();
  return insets;
}

void Textfield::AccessibilitySetValue(const base::string16& new_value) {
  if (!read_only()) {
    SetText(new_value);
    ClearSelection();
  }
}

}  // namespace views
