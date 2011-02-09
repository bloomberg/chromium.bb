// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/textfield.h"

#if defined(OS_LINUX)
#include <gdk/gdkkeysyms.h>
#endif

#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/insets.h"
#include "views/controls/native/native_view_host.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#include "views/widget/widget.h"

#if defined(OS_LINUX)
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#elif defined(OS_WIN)
#include "base/win/win_util.h"
// TODO(beng): this should be removed when the OS_WIN hack from
// ViewHierarchyChanged is removed.
#include "views/controls/textfield/native_textfield_win.h"
#include "views/controls/textfield/native_textfield_views.h"
#endif

namespace views {

// static
const char Textfield::kViewClassName[] = "views/Textfield";

/////////////////////////////////////////////////////////////////////////////
// TextRange

TextRange::TextRange(size_t start, size_t end)
    : start_(start),
      end_(end) {
}

size_t TextRange::GetMin() const {
  return std::min(start_, end_);
}

size_t TextRange::GetMax() const {
  return std::max(start_, end_);
}

void TextRange::SetRange(size_t start, size_t end) {
  start_ = start;
  end_ = end;
}

/////////////////////////////////////////////////////////////////////////////
// Textfield

Textfield::Textfield()
    : native_wrapper_(NULL),
      controller_(NULL),
      style_(STYLE_DEFAULT),
      read_only_(false),
      default_width_in_chars_(0),
      draw_border_(true),
      text_color_(SK_ColorBLACK),
      use_default_text_color_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      num_lines_(1),
      initialized_(false),
      horizontal_margins_were_set_(false),
      vertical_margins_were_set_(false) {
  SetFocusable(true);
}

Textfield::Textfield(StyleFlags style)
    : native_wrapper_(NULL),
      controller_(NULL),
      style_(style),
      read_only_(false),
      default_width_in_chars_(0),
      draw_border_(true),
      text_color_(SK_ColorBLACK),
      use_default_text_color_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      num_lines_(1),
      initialized_(false),
      horizontal_margins_were_set_(false),
      vertical_margins_were_set_(false) {
  SetFocusable(true);
}

Textfield::~Textfield() {
}

void Textfield::SetController(Controller* controller) {
  controller_ = controller;
}

Textfield::Controller* Textfield::GetController() const {
  return controller_;
}

void Textfield::SetReadOnly(bool read_only) {
  read_only_ = read_only;
  if (native_wrapper_) {
    native_wrapper_->UpdateReadOnly();
    native_wrapper_->UpdateTextColor();
    native_wrapper_->UpdateBackgroundColor();
  }
}

bool Textfield::IsPassword() const {
  return style_ & STYLE_PASSWORD;
}

void Textfield::SetPassword(bool password) {
  if (password)
    style_ = static_cast<StyleFlags>(style_ | STYLE_PASSWORD);
  else
    style_ = static_cast<StyleFlags>(style_ & ~STYLE_PASSWORD);
  if (native_wrapper_)
    native_wrapper_->UpdateIsPassword();
}

bool Textfield::IsMultiLine() const {
  return !!(style_ & STYLE_MULTILINE);
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

void Textfield::SelectAll() {
  if (native_wrapper_)
    native_wrapper_->SelectAll();
}

string16 Textfield::GetSelectedText() const {
  if (native_wrapper_)
    return native_wrapper_->GetSelectedText();
  return string16();
}

void Textfield::ClearSelection() const {
  if (native_wrapper_)
    native_wrapper_->ClearSelection();
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

void Textfield::SetFont(const gfx::Font& font) {
  font_ = font;
  if (native_wrapper_)
    native_wrapper_->UpdateFont();
  PreferredSizeChanged();
}

void Textfield::SetHorizontalMargins(int left, int right) {
  margins_.Set(margins_.top(), left, margins_.bottom(), right);
  horizontal_margins_were_set_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateHorizontalMargins();
  PreferredSizeChanged();
}

void Textfield::SetVerticalMargins(int top, int bottom) {
  margins_.Set(top, margins_.left(), bottom, margins_.right());
  vertical_margins_were_set_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateVerticalMargins();
  PreferredSizeChanged();
}

void Textfield::SetHeightInLines(int num_lines) {
  DCHECK(IsMultiLine());
  num_lines_ = num_lines;
  PreferredSizeChanged();
}

void Textfield::RemoveBorder() {
  if (!draw_border_)
    return;

  draw_border_ = false;
  if (native_wrapper_)
    native_wrapper_->UpdateBorder();
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
    native_wrapper_->UpdateIsPassword();
    native_wrapper_->UpdateHorizontalMargins();
    native_wrapper_->UpdateVerticalMargins();
  }
}

void Textfield::SyncText() {
  if (native_wrapper_)
    text_ = native_wrapper_->GetText();
}

bool Textfield::IsIMEComposing() const {
  return native_wrapper_ && native_wrapper_->IsIMEComposing();
}

void Textfield::GetSelectedRange(TextRange* range) const {
  DCHECK(native_wrapper_);
  if (native_wrapper_)
    native_wrapper_->GetSelectedRange(range);
}

void Textfield::SelectRange(const TextRange& range) {
  DCHECK(native_wrapper_);
  if (native_wrapper_)
    native_wrapper_->SelectRange(range);
}

size_t Textfield::GetCursorPosition() const {
  DCHECK(native_wrapper_);
  return native_wrapper_ ? native_wrapper_->GetCursorPosition() : 0;
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, View overrides:

void Textfield::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBoundsRect(GetLocalBounds());
    native_wrapper_->GetView()->Layout();
  }
}

gfx::Size Textfield::GetPreferredSize() {
  gfx::Insets insets;
  if (draw_border_ && native_wrapper_)
    insets = native_wrapper_->CalculateInsets();
  return gfx::Size(font_.GetExpectedTextWidth(default_width_in_chars_) +
                       insets.width(),
                   num_lines_ * font_.GetHeight() + insets.height());
}

bool Textfield::IsFocusable() const {
  return View::IsFocusable() && !read_only_;
}

void Textfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll();
}

bool Textfield::SkipDefaultKeyEventProcessing(const KeyEvent& e) {
  // TODO(hamaji): Figure out which keyboard combinations we need to add here,
  //               similar to LocationBarView::SkipDefaultKeyEventProcessing.
  ui::KeyboardCode key = e.GetKeyCode();
  if (key == ui::VKEY_BACK)
    return true;  // We'll handle BackSpace ourselves.

#if defined(OS_WIN)
  // We don't translate accelerators for ALT + NumPad digit on Windows, they are
  // used for entering special characters.  We do translate alt-home.
  if (e.IsAltDown() && (key != ui::VKEY_HOME) &&
      NativeTextfieldWin::IsNumPadDigit(key, e.IsExtendedKey()))
    return true;
#endif
  return false;
}

void Textfield::PaintFocusBorder(gfx::Canvas* canvas) {
  if (NativeViewHost::kRenderNativeControlFocus)
    View::PaintFocusBorder(canvas);
}

bool Textfield::OnKeyPressed(const views::KeyEvent& e) {
  return native_wrapper_ && native_wrapper_->HandleKeyPressed(e);
}

bool Textfield::OnKeyReleased(const views::KeyEvent& e) {
  return native_wrapper_ && native_wrapper_->HandleKeyReleased(e);
}

void Textfield::WillGainFocus() {
  if (native_wrapper_)
    native_wrapper_->HandleWillGainFocus();
}

void Textfield::DidGainFocus() {
  if (native_wrapper_)
    native_wrapper_->HandleDidGainFocus();
}

void Textfield::WillLoseFocus() {
  if (native_wrapper_)
    native_wrapper_->HandleWillLoseFocus();
}

AccessibilityTypes::Role Textfield::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_TEXT;
}

AccessibilityTypes::State Textfield::GetAccessibleState() {
  int state = 0;
  if (read_only())
    state |= AccessibilityTypes::STATE_READONLY;
  if (IsPassword())
    state |= AccessibilityTypes::STATE_PROTECTED;
  return state;
}

string16 Textfield::GetAccessibleValue() {
  if (!text_.empty())
    return text_;
  return string16();
}

void Textfield::SetEnabled(bool enabled) {
  View::SetEnabled(enabled);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void Textfield::Focus() {
  // Forward the focus to the wrapper if it exists.
  if (!native_wrapper_ || !native_wrapper_->SetFocus()) {
    // If there is no wrapper or the wrapper din't take focus, call
    // View::Focus to clear the native focus so that we still get
    // keyboard messages.
    View::Focus();
  }
}

void Textfield::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_wrapper_ && GetWidget() && !initialized_) {
    initialized_ = true;

    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = NativeTextfieldWrapper::CreateWrapper(this);
    AddChildView(native_wrapper_->GetView());
    // TODO(beng): Move this initialization to NativeTextfieldWin once it
    //             subclasses NativeControlWin.
    UpdateAllProperties();

#if defined(OS_WIN)
    if (!NativeTextfieldViews::IsTextfieldViewsEnabled()) {
      // TODO(beng): remove this once NativeTextfieldWin subclasses
      // NativeControlWin. This is currently called to perform post-AddChildView
      // initialization for the wrapper. The GTK version subclasses things
      // correctly and doesn't need this.
      //
      // Remove the include for native_textfield_win.h above when you fix this.
      static_cast<NativeTextfieldWin*>(native_wrapper_)->AttachHack();
    }
#endif
  }
}

std::string Textfield::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
