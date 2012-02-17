// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/singleton_hwnd.h"

#include "base/memory/singleton.h"
#include "base/process_util.h"
#include "base/win/wrapped_window_proc.h"
#include "ui/base/win/hwnd_util.h"

namespace {

// Windows class name to use for the listener window.
const wchar_t kWindowClassName[] = L"Chrome_SingletonHwnd";

// Windows callback for listening for WM_* messages.
LRESULT CALLBACK ListenerWindowProc(HWND hwnd,
                                    UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  ui::SingletonHwnd::GetInstance()->OnWndProc(hwnd, message, wparam, lparam);
  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

// Creates a listener window to receive WM_* messages.
HWND CreateListenerWindow() {
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = base::win::WrappedWindowProc<ListenerWindowProc>;
  wc.hInstance = base::GetModuleFromAddress(&ListenerWindowProc);
  wc.lpszClassName = kWindowClassName;
  ATOM window_class = ::RegisterClassEx(&wc);
  DCHECK(window_class);

  return ::CreateWindow(MAKEINTATOM(window_class), 0, 0, 0, 0, 0, 0, 0, 0,
                        wc.hInstance, 0);
}

}  // namespace

namespace ui {

// static
SingletonHwnd* SingletonHwnd::GetInstance() {
  return Singleton<SingletonHwnd>::get();
}

void SingletonHwnd::AddObserver(Observer* observer) {
  if (!listener_window_) {
    listener_window_ = CreateListenerWindow();
    ui::CheckWindowCreated(listener_window_);
  }
  observer_list_.AddObserver(observer);
}

void SingletonHwnd::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SingletonHwnd::OnWndProc(HWND hwnd,
                              UINT message,
                              WPARAM wparam,
                              LPARAM lparam) {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnWndProc(hwnd, message, wparam, lparam));
}

SingletonHwnd::SingletonHwnd()
    : listener_window_(NULL) {
}

SingletonHwnd::~SingletonHwnd() {
  if (listener_window_) {
    ::DestroyWindow(listener_window_);
    ::UnregisterClass(kWindowClassName, GetModuleHandle(NULL));
  }
}

}  // namespace ui
