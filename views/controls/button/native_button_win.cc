// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/button/native_button_win.h"

#include <commctrl.h>
#include <oleacc.h>

#include "base/logging.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, public:

NativeButtonWin::NativeButtonWin(NativeButton* native_button)
    : native_button_(native_button),
      button_size_valid_(false) {
  // Associates the actual HWND with the native_button so the native_button is
  // the one considered as having the focus (not the wrapper) when the HWND is
  // focused directly (with a click for example).
  set_focus_view(native_button);
}

NativeButtonWin::~NativeButtonWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, NativeButtonWrapper implementation:

void NativeButtonWin::UpdateLabel() {
  // Show or hide the shield icon of Windows onto this button every time when we
  // update the button text so Windows can lay out the shield icon and the
  // button text correctly.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA &&
      base::win::UserAccountControlIsEnabled()) {
    Button_SetElevationRequiredState(native_view(),
                                     native_button_->need_elevation());
  }

  SetWindowText(native_view(), native_button_->label().c_str());
  button_size_valid_ = false;
}

void NativeButtonWin::UpdateFont() {
  SendMessage(native_view(), WM_SETFONT,
              reinterpret_cast<WPARAM>(native_button_->font().GetNativeFont()),
              FALSE);
  button_size_valid_ = false;
}

void NativeButtonWin::UpdateEnabled() {
  SetEnabled(native_button_->IsEnabled());
}

void NativeButtonWin::UpdateDefault() {
  if (!IsCheckbox()) {
    SendMessage(native_view(), BM_SETSTYLE,
                native_button_->is_default() ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON,
                true);
    button_size_valid_ = false;
  }
}

void NativeButtonWin::UpdateAccessibleName() {
  ui::AccessibleViewState state;
  native_button_->GetAccessibleState(&state);
  string16 name = state.name;
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(
      CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (SUCCEEDED(hr)) {
    VARIANT var;
    var.vt = VT_BSTR;
    var.bstrVal = SysAllocString(name.c_str());
    hr = pAccPropServices->SetHwndProp(native_view(), OBJID_WINDOW,
                                       CHILDID_SELF, PROPID_ACC_NAME, var);
  }
}

View* NativeButtonWin::GetView() {
  return this;
}

void NativeButtonWin::SetFocus() {
  // Focus the associated HWND.
  OnFocus();
}

bool NativeButtonWin::UsesNativeLabel() const {
  return true;
}

bool NativeButtonWin::UsesNativeRadioButtonGroup() const {
  return false;
}

gfx::NativeView NativeButtonWin::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, View overrides:

gfx::Size NativeButtonWin::GetPreferredSize() {
  if (!button_size_valid_ && native_view()) {
    SIZE sz = {0};
    Button_GetIdealSize(native_view(), reinterpret_cast<LPARAM>(&sz));
    button_size_.SetSize(sz.cx, sz.cy);
    button_size_valid_ = true;
  }
  return button_size_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWin, NativeControlWin overrides:

bool NativeButtonWin::ProcessMessage(UINT message, WPARAM w_param,
                                     LPARAM l_param, LRESULT* result) {
  if (message == WM_COMMAND && HIWORD(w_param) == BN_CLICKED) {
    native_button_->ButtonPressed();
    *result = 0;
    return true;
  }
  return NativeControlWin::ProcessMessage(message, w_param, l_param, result);
}

bool NativeButtonWin::OnKeyDown(int vkey) {
  bool enter_pressed = vkey == VK_RETURN;
  if (enter_pressed)
    native_button_->ButtonPressed();
  return enter_pressed;
}

void NativeButtonWin::CreateNativeControl() {
  DWORD flags = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON;
  if (native_button_->is_default())
    flags |= BS_DEFPUSHBUTTON;
  HWND control_hwnd = CreateWindowEx(GetAdditionalExStyle(), L"BUTTON", L"",
                                     flags, 0, 0, width(), height(),
                                     GetWidget()->GetNativeView(), NULL, NULL,
                                     NULL);
  NativeControlCreated(control_hwnd);
}

void NativeButtonWin::NativeControlCreated(HWND control_hwnd) {
  NativeControlWin::NativeControlCreated(control_hwnd);

  UpdateFont();
  UpdateLabel();
  UpdateDefault();
  UpdateAccessibleName();
}

// We could obtain this from the theme, but that only works if themes are
// active.
static const int kCheckboxSize = 13; // pixels

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, public:

NativeCheckboxWin::NativeCheckboxWin(Checkbox* checkbox)
    : NativeButtonWin(checkbox),
      checkbox_(checkbox) {
}

NativeCheckboxWin::~NativeCheckboxWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, View overrides:

gfx::Size NativeCheckboxWin::GetPreferredSize() {
  return gfx::Size(kCheckboxSize, kCheckboxSize);
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, NativeButtonWrapper implementation:

void NativeCheckboxWin::UpdateChecked() {
  SendMessage(native_view(), BM_SETCHECK,
              checkbox_->checked() ? BST_CHECKED : BST_UNCHECKED, 0);
}

void NativeCheckboxWin::SetPushed(bool pushed) {
  SendMessage(native_view(), BM_SETSTATE, pushed, 0);
}

bool NativeCheckboxWin::OnKeyDown(int vkey) {
  // Override the NativeButtonWin behavior which triggers the button on enter
  // key presses when focused.
  return false;
}

bool NativeCheckboxWin::UsesNativeLabel() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, NativeButtonWin overrides:

bool NativeCheckboxWin::ProcessMessage(UINT message, WPARAM w_param,
                                       LPARAM l_param, LRESULT* result) {
  if (message == WM_COMMAND && HIWORD(w_param) == BN_CLICKED) {
    if (!IsRadioButton() || !checkbox_->checked())
      checkbox_->SetChecked(!checkbox_->checked());
    // Fall through to the NativeButtonWin's handler, which will send the
    // clicked notification to the listener...
  }
  return NativeButtonWin::ProcessMessage(message, w_param, l_param, result);
}

////////////////////////////////////////////////////////////////////////////////
// NativeCheckboxWin, protected:

void NativeCheckboxWin::CreateNativeControl() {
  HWND control_hwnd = CreateWindowEx(
      GetAdditionalExStyle(), L"BUTTON", L"",
      WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_CHECKBOX,
      0, 0, width(), height(), GetWidget()->GetNativeView(), NULL, NULL, NULL);
  NativeControlCreated(control_hwnd);
}

void NativeCheckboxWin::NativeControlCreated(HWND control_hwnd) {
  NativeButtonWin::NativeControlCreated(control_hwnd);
  UpdateChecked();
}

////////////////////////////////////////////////////////////////////////////////
// NativeRadioButtonWin, public:

NativeRadioButtonWin::NativeRadioButtonWin(RadioButton* radio_button)
    : NativeCheckboxWin(radio_button) {
}

NativeRadioButtonWin::~NativeRadioButtonWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeRadioButtonWin, NativeCheckboxWin overrides:

void NativeRadioButtonWin::CreateNativeControl() {
  HWND control_hwnd = CreateWindowEx(
      GetAdditionalExStyle(), L"BUTTON",
      L"", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_RADIOBUTTON,
      0, 0, width(), height(), GetWidget()->GetNativeView(), NULL, NULL, NULL);
  NativeControlCreated(control_hwnd);
}

////////////////////////////////////////////////////////////////////////////////
// NativeButtonWrapper, public:

// static
int NativeButtonWrapper::GetFixedWidth() {
  return kCheckboxSize;
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateNativeButtonWrapper(
    NativeButton* native_button) {
  return new NativeButtonWin(native_button);
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateCheckboxWrapper(
    Checkbox* checkbox) {
  return new NativeCheckboxWin(checkbox);
}

// static
NativeButtonWrapper* NativeButtonWrapper::CreateRadioButtonWrapper(
    RadioButton* radio_button) {
  return new NativeRadioButtonWin(radio_button);
}

}  // namespace views
