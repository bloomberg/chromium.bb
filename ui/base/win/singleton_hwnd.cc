// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/singleton_hwnd.h"

#include "base/memory/singleton.h"

namespace ui {

// static
SingletonHwnd* SingletonHwnd::GetInstance() {
  return Singleton<SingletonHwnd>::get();
}

void SingletonHwnd::AddObserver(Observer* observer) {
  if (!hwnd())
    WindowImpl::Init(NULL, gfx::Rect());
  observer_list_.AddObserver(observer);
}

void SingletonHwnd::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

BOOL SingletonHwnd::ProcessWindowMessage(HWND window,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         LRESULT& result,
                                         DWORD msg_map_id) {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnWndProc(window, message, wparam, lparam));
  return false;
}

SingletonHwnd::SingletonHwnd() {
}

SingletonHwnd::~SingletonHwnd() {
}

}  // namespace ui
