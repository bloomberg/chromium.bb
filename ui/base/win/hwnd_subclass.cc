// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/hwnd_subclass.h"

#include "base/logging.h"
#include "ui/base/win/hwnd_util.h"

namespace {
const char kHWNDSubclassKey[] = "__UI_BASE_WIN_HWND_SUBCLASS_PROC__";

LRESULT CALLBACK WndProc(HWND hwnd,
                         UINT message,
                         WPARAM w_param,
                         LPARAM l_param) {
  ui::HWNDSubclass* wrapped_wnd_proc =
      reinterpret_cast<ui::HWNDSubclass*>(
          ui::ViewProp::GetValue(hwnd, kHWNDSubclassKey));
  return wrapped_wnd_proc ? wrapped_wnd_proc->OnWndProc(hwnd,
                                                        message,
                                                        w_param,
                                                        l_param)
                          : DefWindowProc(hwnd, message, w_param, l_param);
}

WNDPROC GetCurrentWndProc(HWND target) {
  return reinterpret_cast<WNDPROC>(GetWindowLong(target, GWL_WNDPROC));
}

}  // namespace

namespace ui {

HWNDSubclass::HWNDSubclass(HWND target)
    : target_(target),
      original_wnd_proc_(GetCurrentWndProc(target)),
      ALLOW_THIS_IN_INITIALIZER_LIST(prop_(target, kHWNDSubclassKey, this)) {
  ui::SetWindowProc(target_, &WndProc);
}

HWNDSubclass::~HWNDSubclass() {
}

void HWNDSubclass::SetFilter(HWNDMessageFilter* filter) {
  filter_.reset(filter);
}

LRESULT HWNDSubclass::OnWndProc(HWND hwnd,
                                UINT message,
                                WPARAM w_param,
                                LPARAM l_param) {
  if (filter_.get()) {
    LRESULT l_result = 0;
    if (filter_->FilterMessage(hwnd, message, w_param, l_param, &l_result))
      return l_result;
  }

  // In most cases, |original_wnd_proc_| will take care of calling
  // DefWindowProc.
  return CallWindowProc(original_wnd_proc_, hwnd, message, w_param, l_param);
}

}  // namespace ui
