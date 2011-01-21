// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/listbox/native_listbox_win.h"

#include <commctrl.h>
#include <windowsx.h>

#include "base/utf_string_conversions.h"
#include "gfx/font.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/listbox/listbox.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeListboxWin, public:

NativeListboxWin::NativeListboxWin(Listbox* listbox,
                                   const std::vector<string16>& strings,
                                   Listbox::Listener* listener)
    : listbox_(listbox),
      strings_(strings),
      listener_(listener) {
  // Associates the actual HWND with the listbox so the listbox is the one
  // considered as having the focus (not the wrapper) when the HWND is
  // focused directly (with a click for example).
  set_focus_view(listbox);
}

NativeListboxWin::~NativeListboxWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeListboxWin, NativeListboxWrapper implementation:

int NativeListboxWin::GetRowCount() const {
  if (!native_view())
    return 0;
  return ListBox_GetCount(native_view());
}

int NativeListboxWin::SelectedRow() const {
  if (!native_view())
    return -1;
  return ListBox_GetCurSel(native_view());
}

void NativeListboxWin::SelectRow(int row) {
  if (!native_view())
    return;
  ListBox_SetCurSel(native_view(), row);
}

View* NativeListboxWin::GetView() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeListboxWin, View overrides:

gfx::Size NativeListboxWin::GetPreferredSize() {
  SIZE sz = {0};
  SendMessage(native_view(), BCM_GETIDEALSIZE, 0,
              reinterpret_cast<LPARAM>(&sz));

  return gfx::Size(sz.cx, sz.cy);
}

////////////////////////////////////////////////////////////////////////////////
// NativeListboxWin, NativeControlWin overrides:

bool NativeListboxWin::ProcessMessage(UINT message, WPARAM w_param,
                                      LPARAM l_param, LRESULT* result) {
  if (message == WM_COMMAND) {
    switch (HIWORD(w_param)) {
      case LBN_SELCHANGE:
        if (listener_)
          listener_->ListboxSelectionChanged(listbox_);
        return true;
      default:
        break;
    }
  }

  return NativeControlWin::ProcessMessage(message, w_param, l_param, result);
}

////////////////////////////////////////////////////////////////////////////////
// NativeListboxWin, protected:

void NativeListboxWin::CreateNativeControl() {
  int style = WS_CHILD | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY;
  // If there's only one column and the title string is empty, don't show a
  // header.
  HWND hwnd = ::CreateWindowEx(WS_EX_CLIENTEDGE | GetAdditionalRTLStyle(),
                               WC_LISTBOX,
                               L"",
                               style,
                               0, 0, width(), height(),
                               listbox_->GetWidget()->GetNativeView(),
                               NULL, NULL, NULL);
  HFONT font = ResourceBundle::GetSharedInstance().
      GetFont(ResourceBundle::BaseFont).GetNativeFont();
  SendMessage(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
  l10n_util::AdjustUIFontForWindow(hwnd);

  for (size_t i = 0; i < strings_.size(); ++i)
    ListBox_AddString(hwnd, UTF16ToWide(strings_[i]).c_str());

  NativeControlCreated(hwnd);

  // Bug 964884: detach the IME attached to this window.
  // We should attach IMEs only when we need to input CJK strings.
  ::ImmAssociateContextEx(hwnd, NULL, 0);
}

////////////////////////////////////////////////////////////////////////////////
// NativeListboxWrapper, public:

// static
NativeListboxWrapper* NativeListboxWrapper::CreateNativeWrapper(
    Listbox* listbox,
    const std::vector<string16>& strings,
    Listbox::Listener* listener) {
  return new NativeListboxWin(listbox, strings, listener);
}

}  // namespace views
