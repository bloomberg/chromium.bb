// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/native_button.h"

#include "base/i18n/rtl.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/controls/native/native_view_host.h"

#if defined(OS_WIN)
#include "ui/gfx/platform_font_win.h"
#elif defined(OS_LINUX)
#include <gdk/gdkkeysyms.h>
#include "views/screen.h"
#endif


namespace views {

#if defined(OS_WIN)
// The min size in DLUs comes from
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
static const int kMinWidthDLUs = 50;
static const int kMinHeightDLUs = 14;

// Horizontal padding (on each side).
static const int kButtonBorderHWidth = 8;
#else
// Horizontal padding (on each side).
static const int kButtonBorderHWidth = 0;
#endif

// static
const char NativeButtonBase::kViewClassName[] = "views/NativeButtonBase";

////////////////////////////////////////////////////////////////////////////////
// NativeButtonBase, public:

NativeButtonBase::NativeButtonBase(ButtonListener* listener)
    : Button(listener),
      native_wrapper_(NULL),
      is_default_(false),
      need_elevation_(false),
      ignore_minimum_size_(false) {
  InitBorder();
  SetFocusable(true);
}

NativeButtonBase::NativeButtonBase(ButtonListener* listener,
                                   const std::wstring& label)
    : Button(listener),
      native_wrapper_(NULL),
      is_default_(false),
      need_elevation_(false),
      ignore_minimum_size_(false) {
  SetLabel(label);  // SetLabel takes care of label layout in RTL UI.
  InitBorder();
  SetFocusable(true);
}

NativeButtonBase::~NativeButtonBase() {
}

void NativeButtonBase::SetLabel(const std::wstring& label) {
  label_ = WideToUTF16Hack(label);

  // Even though we create a flipped HWND for a native button when the locale
  // is right-to-left, Windows does not render text for the button using a
  // right-to-left context (perhaps because the parent HWND is not flipped).
  // The result is that RTL strings containing punctuation marks are not
  // displayed properly. For example, the string "...ABC" (where A, B and C are
  // Hebrew characters) is displayed as "ABC..." which is incorrect.
  //
  // In order to overcome this problem, we mark the localized Hebrew strings as
  // RTL strings explicitly (using the appropriate Unicode formatting) so that
  // Windows displays the text correctly regardless of the HWND hierarchy.
  base::i18n::AdjustStringForLocaleDirection(&label_);

  if (native_wrapper_)
    native_wrapper_->UpdateLabel();

  // Update the accessible name whenever the label changes.
  SetAccessibleName(WideToUTF16Hack(label));
  PreferredSizeChanged();
}

void NativeButtonBase::SetIsDefault(bool is_default) {
  if (is_default == is_default_)
    return;
  is_default_ = is_default;
  if (is_default_)
    AddAccelerator(Accelerator(ui::VKEY_RETURN, false, false, false));
  else
    RemoveAccelerator(Accelerator(ui::VKEY_RETURN, false, false, false));
  if (native_wrapper_)
    native_wrapper_->UpdateDefault();
  PreferredSizeChanged();
}

void NativeButtonBase::SetNeedElevation(bool need_elevation) {
  need_elevation_ = need_elevation;
  if (native_wrapper_)
    native_wrapper_->UpdateLabel();
  PreferredSizeChanged();
}

void NativeButtonBase::ButtonPressed() {
  RequestFocus();

  // TODO(beng): obtain mouse event flags for native buttons someday.
#if defined(OS_WIN)
  DWORD pos = GetMessagePos();
  POINTS points = MAKEPOINTS(pos);
  gfx::Point cursor_point(points.x, points.y);
#elif defined(OS_LINUX)
  gfx::Point cursor_point = Screen::GetCursorScreenPoint();
#endif

  views::MouseEvent event(ui::ET_MOUSE_RELEASED,
                          cursor_point.x(), cursor_point.y(),
                          ui::EF_LEFT_BUTTON_DOWN);
  NotifyClick(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonBase, View overrides:

gfx::Size NativeButtonBase::GetPreferredSize() {
  if (!native_wrapper_)
    return gfx::Size();

  gfx::Size sz = native_wrapper_->GetView()->GetPreferredSize();

  // Add in the border size. (Do this before clamping the minimum size in case
  // that clamping causes an increase in size that would include the borders.
  gfx::Insets border = GetInsets();
  sz.set_width(sz.width() + border.left() + border.right());
  sz.set_height(sz.height() + border.top() + border.bottom());

#if defined(OS_WIN)
  // Clamp the size returned to at least the minimum size.
  if (!ignore_minimum_size_) {
    gfx::PlatformFontWin* platform_font =
        static_cast<gfx::PlatformFontWin*>(font_.platform_font());
    sz.set_width(std::max(
        sz.width(),
        platform_font->horizontal_dlus_to_pixels(kMinWidthDLUs)));
    sz.set_height(std::max(
        sz.height(),
        platform_font->vertical_dlus_to_pixels(kMinHeightDLUs)));
  }
  // GTK returns a meaningful preferred size so that we don't need to adjust
  // the preferred size as we do on windows.
#endif

  return sz;
}

void NativeButtonBase::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
    native_wrapper_->GetView()->Layout();
  }
}

void NativeButtonBase::SetEnabled(bool flag) {
  Button::SetEnabled(flag);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void NativeButtonBase::ViewHierarchyChanged(bool is_add, View* parent,
                                         View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = CreateWrapper();
    UpdateAllStates();
    AddChildView(native_wrapper_->GetView());
  }
}

std::string NativeButtonBase::GetClassName() const {
  return kViewClassName;
}

bool NativeButtonBase::AcceleratorPressed(const Accelerator& accelerator) {
  if (IsEnabled()) {
#if defined(OS_WIN)
    DWORD pos = GetMessagePos();
    POINTS points = MAKEPOINTS(pos);
    gfx::Point cursor_point(points.x, points.y);
#elif defined(OS_LINUX)
    gfx::Point cursor_point = Screen::GetCursorScreenPoint();
#endif
    views::MouseEvent event(ui::ET_MOUSE_RELEASED,
                            cursor_point.x(), cursor_point.y(),
                            ui::EF_LEFT_BUTTON_DOWN);
    NotifyClick(event);
    return true;
  }
  return false;
}

void NativeButtonBase::OnFocus() {
  // Forward the focus to the wrapper.
  if (native_wrapper_)
    native_wrapper_->SetFocus();
  else
    Button::OnFocus();  // Will focus the RootView window (so we still get
                        // keyboard messages).
}

void NativeButtonBase::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (NativeViewHost::kRenderNativeControlFocus)
    View::OnPaintFocusBorder(canvas);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonBase, protected:

NativeButtonWrapper* NativeButtonBase::CreateWrapper() {
  return NativeButtonWrapper::CreateNativeButtonWrapper(this);
}

void NativeButtonBase::InitBorder() {
  set_border(Border::CreateEmptyBorder(0, kButtonBorderHWidth, 0,
                                       kButtonBorderHWidth));
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonBase, private:
void NativeButtonBase::UpdateAllStates() {
  native_wrapper_->UpdateLabel();
  native_wrapper_->UpdateFont();
  native_wrapper_->UpdateEnabled();
  native_wrapper_->UpdateDefault();
  native_wrapper_->UpdateChecked();
  native_wrapper_->UpdateAccessibleName();
}

#if defined(TOUCH_UI)
////////////////////////////////////////////////////////////////////////////////
// NativeButton, public:

NativeButton::NativeButton(ButtonListener* listener, const std::wstring& label)
    : TextButton(listener, label), is_default_(false) {
  set_alignment(TextButton::ALIGN_CENTER);
  SetNormalHasBorder(true);
  SetFocusable(true);
}

void NativeButton::SetLabel(const std::wstring& label) {
  SetText(label);
}

void NativeButton::set_font(const gfx::Font& font) {
  SetFont(font);
}

void NativeButton::SetIsDefault(bool default_button) {
  is_default_ = default_button;
}

bool NativeButton::is_default() const {
  return is_default_;
}

void NativeButton::set_ignore_minimum_size(bool) {
    // TODO(saintlou): ignoring for now
}
#else
////////////////////////////////////////////////////////////////////////////////
// NativeButton, public:

NativeButton::NativeButton(ButtonListener* listener)
    : NativeButtonBase(listener) {
}

NativeButton::NativeButton(ButtonListener* listener, const std::wstring& label)
    : NativeButtonBase(listener, label) {
}

NativeButton::~NativeButton() {
}
#endif

}  // namespace views
