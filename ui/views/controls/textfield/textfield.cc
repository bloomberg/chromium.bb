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
#include "ui/views/controls/textfield/native_textfield_wrapper.h"
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
    : native_wrapper_(NULL),
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
    : native_wrapper_(NULL),
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
  if (native_wrapper_) {
    native_wrapper_->UpdateReadOnly();
    native_wrapper_->UpdateTextColor();
    native_wrapper_->UpdateBackgroundColor();
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
  if (native_wrapper_)
    native_wrapper_->UpdateIsObscured();
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

void Textfield::SetText(const string16& text) {
  text_ = text;
  if (native_wrapper_)
    native_wrapper_->UpdateText();
}

void Textfield::AppendText(const string16& text) {
  text_ += text;
  if (native_wrapper_)
    native_wrapper_->AppendText(text);
}

void Textfield::InsertOrReplaceText(const string16& text) {
  if (native_wrapper_) {
    native_wrapper_->InsertOrReplaceText(text);
    text_ = native_wrapper_->GetText();
  }
}

base::i18n::TextDirection Textfield::GetTextDirection() const {
  return native_wrapper_ ?
      native_wrapper_->GetTextDirection() : base::i18n::UNKNOWN_DIRECTION;
}

void Textfield::SelectAll(bool reversed) {
  if (native_wrapper_)
    native_wrapper_->SelectAll(reversed);
}

string16 Textfield::GetSelectedText() const {
  return native_wrapper_ ? native_wrapper_->GetSelectedText() : string16();
}

void Textfield::ClearSelection() const {
  if (native_wrapper_)
    native_wrapper_->ClearSelection();
}

bool Textfield::HasSelection() const {
  return native_wrapper_ && !native_wrapper_->GetSelectedRange().is_empty();
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
  if (native_wrapper_)
    native_wrapper_->UpdateTextColor();
}

void Textfield::UseDefaultTextColor() {
  use_default_text_color_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateTextColor();
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
  if (native_wrapper_)
    native_wrapper_->UpdateBackgroundColor();
}

void Textfield::UseDefaultBackgroundColor() {
  use_default_background_color_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateBackgroundColor();
}

bool Textfield::GetCursorEnabled() const {
  return native_wrapper_ && native_wrapper_->GetCursorEnabled();
}

void Textfield::SetCursorEnabled(bool enabled) {
  if (native_wrapper_)
    native_wrapper_->SetCursorEnabled(enabled);
}

void Textfield::SetFontList(const gfx::FontList& font_list) {
  font_list_ = font_list;
  if (native_wrapper_)
    native_wrapper_->UpdateFont();
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
  if (native_wrapper_)
    native_wrapper_->UpdateHorizontalMargins();
  PreferredSizeChanged();
}

void Textfield::SetVerticalMargins(int top, int bottom) {
  if (vertical_margins_were_set_ &&
      top == margins_.top() && bottom == margins_.bottom()) {
    return;
  }
  margins_.Set(top, margins_.left(), bottom, margins_.right());
  vertical_margins_were_set_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateVerticalMargins();
  PreferredSizeChanged();
}

void Textfield::RemoveBorder() {
  if (!draw_border_)
    return;

  draw_border_ = false;
  if (native_wrapper_)
    native_wrapper_->UpdateBorder();
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
  if (native_wrapper_) {
    native_wrapper_->UpdateText();
    native_wrapper_->UpdateTextColor();
    native_wrapper_->UpdateBackgroundColor();
    native_wrapper_->UpdateReadOnly();
    native_wrapper_->UpdateFont();
    native_wrapper_->UpdateEnabled();
    native_wrapper_->UpdateBorder();
    native_wrapper_->UpdateIsObscured();
    native_wrapper_->UpdateHorizontalMargins();
    native_wrapper_->UpdateVerticalMargins();
  }
}

void Textfield::SyncText() {
  if (native_wrapper_) {
    string16 new_text = native_wrapper_->GetText();
    if (new_text != text_) {
      text_ = new_text;
      if (controller_)
        controller_->ContentsChanged(this, text_);
    }
  }
}

bool Textfield::IsIMEComposing() const {
  return native_wrapper_ && native_wrapper_->IsIMEComposing();
}

gfx::Range Textfield::GetSelectedRange() const {
  return native_wrapper_->GetSelectedRange();
}

void Textfield::SelectRange(const gfx::Range& range) {
  native_wrapper_->SelectRange(range);
}

gfx::SelectionModel Textfield::GetSelectionModel() const {
  return native_wrapper_->GetSelectionModel();
}

void Textfield::SelectSelectionModel(const gfx::SelectionModel& sel) {
  native_wrapper_->SelectSelectionModel(sel);
}

size_t Textfield::GetCursorPosition() const {
  return native_wrapper_->GetCursorPosition();
}

void Textfield::SetColor(SkColor value) {
  return native_wrapper_->SetColor(value);
}

void Textfield::ApplyColor(SkColor value, const gfx::Range& range) {
  return native_wrapper_->ApplyColor(value, range);
}

void Textfield::SetStyle(gfx::TextStyle style, bool value) {
  return native_wrapper_->SetStyle(style, value);
}

void Textfield::ApplyStyle(gfx::TextStyle style,
                           bool value,
                           const gfx::Range& range) {
  return native_wrapper_->ApplyStyle(style, value, range);
}

void Textfield::ClearEditHistory() {
  native_wrapper_->ClearEditHistory();
}

void Textfield::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

void Textfield::ExecuteCommand(int command_id) {
  native_wrapper_->ExecuteTextCommand(command_id);
}

void Textfield::SetFocusPainter(scoped_ptr<Painter> focus_painter) {
  focus_painter_ = focus_painter.Pass();
}

bool Textfield::HasTextBeingDragged() {
  return native_wrapper_->HasTextBeingDragged();
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, View overrides:

void Textfield::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBoundsRect(GetContentsBounds());
    native_wrapper_->GetView()->Layout();
  }
}

int Textfield::GetBaseline() const {
  gfx::Insets insets = GetTextInsets();
  const int baseline = native_wrapper_ ?
      native_wrapper_->GetTextfieldBaseline() : font_list_.GetBaseline();
  return insets.top() + baseline;
}

gfx::Size Textfield::GetPreferredSize() {
  gfx::Insets insets = GetTextInsets();

  const int font_height = native_wrapper_ ? native_wrapper_->GetFontHeight() :
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
  return native_wrapper_ && native_wrapper_->HandleKeyPressed(e);
}

bool Textfield::OnKeyReleased(const ui::KeyEvent& e) {
  return native_wrapper_ && native_wrapper_->HandleKeyReleased(e);
}

bool Textfield::OnMouseDragged(const ui::MouseEvent& e) {
  if (!e.IsOnlyRightMouseButton())
    return View::OnMouseDragged(e);
  return true;
}

void Textfield::OnFocus() {
  if (native_wrapper_)
    native_wrapper_->HandleFocus();

  // Forward the focus to the wrapper if it exists.
  if (!native_wrapper_ || !native_wrapper_->SetFocus()) {
    // If there is no wrapper or the wrapper didn't take focus, call
    // View::Focus to clear the native focus so that we still get
    // keyboard messages.
    View::OnFocus();
  }

  // Border typically draws focus indicator.
  SchedulePaint();
}

void Textfield::OnBlur() {
  if (native_wrapper_)
    native_wrapper_->HandleBlur();

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

  const gfx::Range range = native_wrapper_->GetSelectedRange();
  state->selection_start = range.start();
  state->selection_end = range.end();

  if (!read_only()) {
    state->set_value_callback =
        base::Bind(&Textfield::AccessibilitySetValue,
                   weak_ptr_factory_.GetWeakPtr());
  }
}

ui::TextInputClient* Textfield::GetTextInputClient() {
  return native_wrapper_ ? native_wrapper_->GetTextInputClient() : NULL;
}

gfx::Point Textfield::GetKeyboardContextMenuLocation() {
  return native_wrapper_ ? native_wrapper_->GetContextMenuLocation() :
                           View::GetKeyboardContextMenuLocation();
}

void Textfield::OnEnabledChanged() {
  View::OnEnabledChanged();
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void Textfield::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !native_wrapper_ && GetWidget()) {
    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = NativeTextfieldWrapper::CreateWrapper(this);
    AddChildViewAt(native_wrapper_->GetView(), 0);
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
  if (draw_border_ && native_wrapper_)
    insets += native_wrapper_->CalculateInsets();
  return insets;
}

void Textfield::AccessibilitySetValue(const string16& new_value) {
  if (!read_only()) {
    SetText(new_value);
    ClearSelection();
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper, public:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  return new NativeTextfieldViews(field);
}

}  // namespace views
