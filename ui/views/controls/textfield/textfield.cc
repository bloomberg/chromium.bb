// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <string>

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/range/range.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/selection_model.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/textfield/native_textfield_views.h"
#include "ui/views/controls/textfield/native_textfield_wrapper.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
// TODO(beng): this should be removed when the OS_WIN hack from
// ViewHierarchyChanged is removed.
#include "ui/views/controls/textfield/native_textfield_win.h"
#endif

namespace {

// Default placeholder text color.
const SkColor kDefaultPlaceholderTextColor = SK_ColorLTGRAY;

#if defined(OS_WIN) && !defined(USE_AURA)
bool UseNativeTextfieldViews() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableViewsTextfield);
}
#endif

}  // namespace

namespace views {

// static
const char Textfield::kViewClassName[] = "views/Textfield";

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
      cursor_color_(SK_ColorBLACK),
      use_default_cursor_color_(true),
      initialized_(false),
      horizontal_margins_were_set_(false),
      vertical_margins_were_set_(false),
      placeholder_text_color_(kDefaultPlaceholderTextColor),
      text_input_type_(ui::TEXT_INPUT_TYPE_TEXT) {
  set_focusable(true);
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
      cursor_color_(SK_ColorBLACK),
      use_default_cursor_color_(true),
      initialized_(false),
      horizontal_margins_were_set_(false),
      vertical_margins_were_set_(false),
      placeholder_text_color_(kDefaultPlaceholderTextColor),
      text_input_type_(ui::TEXT_INPUT_TYPE_TEXT) {
  set_focusable(true);
  if (IsObscured())
    SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
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
  read_only_ = read_only;
  set_focusable(!read_only);
  if (native_wrapper_) {
    native_wrapper_->UpdateReadOnly();
    native_wrapper_->UpdateTextColor();
    native_wrapper_->UpdateBackgroundColor();
    native_wrapper_->UpdateCursorColor();
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

void Textfield::SelectAll(bool reversed) {
  if (native_wrapper_)
    native_wrapper_->SelectAll(reversed);
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

bool Textfield::HasSelection() const {
  ui::Range range;
  if (native_wrapper_)
    native_wrapper_->GetSelectedRange(&range);
  return !range.is_empty();
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

void Textfield::SetCursorColor(SkColor color) {
  cursor_color_ = color;
  use_default_cursor_color_ = false;
  if (native_wrapper_)
    native_wrapper_->UpdateCursorColor();
}

void Textfield::UseDefaultCursorColor() {
  use_default_cursor_color_ = true;
  if (native_wrapper_)
    native_wrapper_->UpdateCursorColor();
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
    native_wrapper_->UpdateCursorColor();
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

void Textfield::GetSelectedRange(ui::Range* range) const {
  DCHECK(native_wrapper_);
  native_wrapper_->GetSelectedRange(range);
}

void Textfield::SelectRange(const ui::Range& range) {
  DCHECK(native_wrapper_);
  native_wrapper_->SelectRange(range);
}

void Textfield::GetSelectionModel(gfx::SelectionModel* sel) const {
  DCHECK(native_wrapper_);
  native_wrapper_->GetSelectionModel(sel);
}

void Textfield::SelectSelectionModel(const gfx::SelectionModel& sel) {
  DCHECK(native_wrapper_);
  native_wrapper_->SelectSelectionModel(sel);
}

size_t Textfield::GetCursorPosition() const {
  DCHECK(native_wrapper_);
  return native_wrapper_->GetCursorPosition();
}

void Textfield::ApplyStyleRange(const gfx::StyleRange& style) {
  DCHECK(native_wrapper_);
  return native_wrapper_->ApplyStyleRange(style);
}

void Textfield::ApplyDefaultStyle() {
  DCHECK(native_wrapper_);
  native_wrapper_->ApplyDefaultStyle();
}

void Textfield::ClearEditHistory() {
  DCHECK(native_wrapper_);
  native_wrapper_->ClearEditHistory();
}

void Textfield::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
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
  // For NativeTextfieldViews, we might use a pre-defined font list (defined in
  // IDS_UI_FONT_FAMILY_CROS) as the fonts to render text. The fonts in the
  // list might be different (in name or in size) from |font_|, so we need to
  // use GetFontHeight() to get the height of the first font in the list to
  // guide textfield's height.
  const int font_height = native_wrapper_ ? native_wrapper_->GetFontHeight() :
                                            font_.GetHeight();
  return gfx::Size(font_.GetExpectedTextWidth(default_width_in_chars_) +
                       insets.width(), font_height + insets.height());
}

void Textfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll(false);
}

bool Textfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // TODO(hamaji): Figure out which keyboard combinations we need to add here,
  //               similar to LocationBarView::SkipDefaultKeyEventProcessing.
  ui::KeyboardCode key = e.key_code();
  if (key == ui::VKEY_BACK)
    return true;  // We'll handle BackSpace ourselves.

#if defined(USE_AURA)
  NOTIMPLEMENTED();
#elif defined(OS_WIN)
  // We don't translate accelerators for ALT + NumPad digit on Windows, they are
  // used for entering special characters.  We do translate alt-home.
  if (e.IsAltDown() && (key != ui::VKEY_HOME) &&
      NativeTextfieldWin::IsNumPadDigit(key,
                                        (e.flags() & ui::EF_EXTENDED) != 0))
    return true;
#endif
  return false;
}

void Textfield::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (NativeViewHost::kRenderNativeControlFocus)
    View::OnPaintFocusBorder(canvas);
}

bool Textfield::OnKeyPressed(const ui::KeyEvent& e) {
  return native_wrapper_ && native_wrapper_->HandleKeyPressed(e);
}

bool Textfield::OnKeyReleased(const ui::KeyEvent& e) {
  return native_wrapper_ && native_wrapper_->HandleKeyReleased(e);
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
}

void Textfield::OnBlur() {
  if (native_wrapper_)
    native_wrapper_->HandleBlur();
}

void Textfield::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TEXT;
  state->name = accessible_name_;
  if (read_only())
    state->state |= ui::AccessibilityTypes::STATE_READONLY;
  if (IsObscured())
    state->state |= ui::AccessibilityTypes::STATE_PROTECTED;
  state->value = text_;

  DCHECK(native_wrapper_);
  ui::Range range;
  native_wrapper_->GetSelectedRange(&range);
  state->selection_start = range.start();
  state->selection_end = range.end();
}

ui::TextInputClient* Textfield::GetTextInputClient() {
  return native_wrapper_ ? native_wrapper_->GetTextInputClient() : NULL;
}

void Textfield::OnEnabledChanged() {
  View::OnEnabledChanged();
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
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

#if defined(OS_WIN) && !defined(USE_AURA)
    // TODO(beng): Remove this once NativeTextfieldWin subclasses
    // NativeControlWin. This is currently called to perform post-AddChildView
    // initialization for the wrapper.
    //
    // Remove the include for native_textfield_win.h above when you fix this.
    if (!UseNativeTextfieldViews())
      static_cast<NativeTextfieldWin*>(native_wrapper_)->AttachHack();
#endif
  }
}

std::string Textfield::GetClassName() const {
  return kViewClassName;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper, public:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
#if defined(OS_WIN) && !defined(USE_AURA)
  if (!UseNativeTextfieldViews())
    return new NativeTextfieldWin(field);
#endif
  return new NativeTextfieldViews(field);
}

}  // namespace views
