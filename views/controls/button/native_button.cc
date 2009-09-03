// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "views/controls/button/native_button.h"

#if defined(OS_WIN)
#include <atlbase.h>
#include <atlapp.h>  // for GET_X/Y_LPARAM
#endif

#if defined(OS_LINUX)
#include <gdk/gdkkeysyms.h>
#include "views/screen.h"
#endif

#include "app/l10n_util.h"
#include "base/logging.h"

namespace views {

static int kButtonBorderHWidth = 8;

// static
const char NativeButton::kViewClassName[] = "views/NativeButton";

////////////////////////////////////////////////////////////////////////////////
// NativeButton, public:

NativeButton::NativeButton(ButtonListener* listener)
    : Button(listener),
      native_wrapper_(NULL),
      is_default_(false),
      ignore_minimum_size_(false),
      minimum_size_(50, 14) {
  // The min size in DLUs comes from
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
  InitBorder();
  SetFocusable(true);
}

NativeButton::NativeButton(ButtonListener* listener, const std::wstring& label)
    : Button(listener),
      native_wrapper_(NULL),
      is_default_(false),
      ignore_minimum_size_(false),
      minimum_size_(50, 14) {
  SetLabel(label);  // SetLabel takes care of label layout in RTL UI.
  // The min size in DLUs comes from
  // http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dnwue/html/ch14e.asp
  InitBorder();
  SetFocusable(true);
}

NativeButton::~NativeButton() {
}

void NativeButton::SetLabel(const std::wstring& label) {
  label_ = label;

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
  std::wstring localized_label;
  if (l10n_util::AdjustStringForLocaleDirection(label_, &localized_label))
    label_ = localized_label;

  if (native_wrapper_)
    native_wrapper_->UpdateLabel();
}

void NativeButton::SetIsDefault(bool is_default) {
#if defined(OS_WIN)
  int return_code = VK_RETURN;
#else
  int return_code = GDK_Return;
#endif
  if (is_default == is_default_)
    return;
  if (is_default)
    AddAccelerator(Accelerator(return_code, false, false, false));
  else
    RemoveAccelerator(Accelerator(return_code, false, false, false));
  SetAppearsAsDefault(is_default);
}

void NativeButton::SetAppearsAsDefault(bool appears_as_default) {
  is_default_ = appears_as_default;
  if (native_wrapper_)
    native_wrapper_->UpdateDefault();
}

void NativeButton::ButtonPressed() {
  RequestFocus();

  // TODO(beng): obtain mouse event flags for native buttons someday.
#if defined(OS_WIN)
  DWORD pos = GetMessagePos();
  gfx::Point cursor_point(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
#elif defined(OS_LINUX)
  gfx::Point cursor_point = Screen::GetCursorScreenPoint();
#endif

  views::MouseEvent event(views::Event::ET_MOUSE_RELEASED,
                          cursor_point.x(), cursor_point.y(),
                          views::Event::EF_LEFT_BUTTON_DOWN);
  NotifyClick(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButton, View overrides:

gfx::Size NativeButton::GetPreferredSize() {
  if (!native_wrapper_)
    return gfx::Size();

  gfx::Size sz = native_wrapper_->GetView()->GetPreferredSize();

  // Add in the border size. (Do this before clamping the minimum size in case
  // that clamping causes an increase in size that would include the borders.
  gfx::Insets border = GetInsets();
  sz.set_width(sz.width() + border.left() + border.right());
  sz.set_height(sz.height() + border.top() + border.bottom());

  // Clamp the size returned to at least the minimum size.
#if defined(OS_WIN)
  if (!ignore_minimum_size_) {
    if (minimum_size_.width()) {
      int min_width = font_.horizontal_dlus_to_pixels(minimum_size_.width());
      sz.set_width(std::max(static_cast<int>(sz.width()), min_width));
    }
    if (minimum_size_.height()) {
      int min_height = font_.vertical_dlus_to_pixels(minimum_size_.height());
      sz.set_height(std::max(static_cast<int>(sz.height()), min_height));
    }
  }
#else
    if (minimum_size_.width() || minimum_size_.height())
      NOTIMPLEMENTED();
#endif

  return sz;
}

void NativeButton::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
    native_wrapper_->GetView()->Layout();
  }
}

void NativeButton::SetEnabled(bool flag) {
  Button::SetEnabled(flag);
  if (native_wrapper_)
    native_wrapper_->UpdateEnabled();
}

void NativeButton::ViewHierarchyChanged(bool is_add, View* parent,
                                         View* child) {
  if (is_add && !native_wrapper_ && GetWidget()) {
    // The native wrapper's lifetime will be managed by the view hierarchy after
    // we call AddChildView.
    native_wrapper_ = CreateWrapper();
    AddChildView(native_wrapper_->GetView());
  }
}

std::string NativeButton::GetClassName() const {
  return kViewClassName;
}

bool NativeButton::AcceleratorPressed(const Accelerator& accelerator) {
  if (IsEnabled()) {
#if defined(OS_WIN)
    DWORD pos = GetMessagePos();
    gfx::Point cursor_point(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
#elif defined(OS_LINUX)
    gfx::Point cursor_point = Screen::GetCursorScreenPoint();
#endif
    views::MouseEvent event(views::Event::ET_MOUSE_RELEASED,
                            cursor_point.x(), cursor_point.y(),
                            views::Event::EF_LEFT_BUTTON_DOWN);
    NotifyClick(event);
    return true;
  }
  return false;
}

void NativeButton::Focus() {
  // Forward the focus to the wrapper.
  if (native_wrapper_)
    native_wrapper_->SetFocus();
  else
    Button::Focus();  // Will focus the RootView window (so we still get
                      // keyboard messages).
}

////////////////////////////////////////////////////////////////////////////////
// NativeButton, protected:

NativeButtonWrapper* NativeButton::CreateWrapper() {
  NativeButtonWrapper* native_wrapper =
      NativeButtonWrapper::CreateNativeButtonWrapper(this);
  native_wrapper->UpdateLabel();
  native_wrapper->UpdateEnabled();
  return native_wrapper;
}

void NativeButton::InitBorder() {
  set_border(Border::CreateEmptyBorder(0, kButtonBorderHWidth, 0,
                                       kButtonBorderHWidth));
}

}  // namespace views
