// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen_compatible_dc_win.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/win/wrapped_window_proc.h"
#include "ui/base/win/hwnd_util.h"

namespace {

// Windows class name to use for the listener window.
const wchar_t kWindowClassName[] = L"CachedCompatibleDC_Invalidator";

class CachedCompatibleDC {
 public:
  // Singleton getter.
  static CachedCompatibleDC* GetInstance();

  // Returns the cached screen compatible DC or creates one if needed.
  HDC GetOrCreateCompatibleDC();

  // Deletes the cached DC, called on WM_DISPLAYCHANGE notifications.
  void DeleteCachedCompatibleDC();

 private:
  // Cached device context compatible with the screen.
  HDC compatible_dc_;

  // HWND for listening to WM_DISPLAYCHANGE notifications.
  HWND listener_window_;

  CachedCompatibleDC();
  ~CachedCompatibleDC();

  friend struct DefaultSingletonTraits<CachedCompatibleDC>;

  DISALLOW_COPY_AND_ASSIGN(CachedCompatibleDC);
};

// Windows callback for listening for WM_DISPLAYCHANGE messages.
LRESULT CALLBACK ListenerWindowProc(HWND hwnd,
                                    UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  if (message == WM_DISPLAYCHANGE)
    CachedCompatibleDC::GetInstance()->DeleteCachedCompatibleDC();

  return ::DefWindowProc(hwnd, message, wparam, lparam);
}

// Creates a listener window to handle WM_DISPLAYCHANGE messages.
HWND CreateListenerWindow() {
  HINSTANCE hinst = 0;
  if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<char*>(&ListenerWindowProc),
                            &hinst)) {
    NOTREACHED();
  }

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = base::win::WrappedWindowProc<ListenerWindowProc>;
  wc.hInstance = hinst;
  wc.lpszClassName = kWindowClassName;
  ATOM clazz = ::RegisterClassEx(&wc);
  DCHECK(clazz);

  return ::CreateWindow(MAKEINTATOM(clazz), 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0,
                        hinst, 0);
}

CachedCompatibleDC::CachedCompatibleDC()
    : compatible_dc_(NULL),
      listener_window_(NULL) {
}

CachedCompatibleDC::~CachedCompatibleDC() {
  DeleteCachedCompatibleDC();
  if (listener_window_) {
    ::DestroyWindow(listener_window_);
    ::UnregisterClass(kWindowClassName, GetModuleHandle(NULL));
  }
}

// static
CachedCompatibleDC* CachedCompatibleDC::GetInstance() {
  return Singleton<CachedCompatibleDC>::get();
}

HDC CachedCompatibleDC::GetOrCreateCompatibleDC() {
  if (!compatible_dc_) {
    compatible_dc_ = ::CreateCompatibleDC(NULL);
    if (!listener_window_) {
      listener_window_ = CreateListenerWindow();
      ui::CheckWindowCreated(listener_window_);
    }
  }
  return compatible_dc_;
}

void CachedCompatibleDC::DeleteCachedCompatibleDC() {
  if (compatible_dc_) {
    ::DeleteDC(compatible_dc_);
    compatible_dc_ = NULL;
  }
}

}  // namespace

namespace gfx {

ScopedTemporaryScreenCompatibleDC::ScopedTemporaryScreenCompatibleDC()
    : hdc_(CachedCompatibleDC::GetInstance()->GetOrCreateCompatibleDC()) {
}

ScopedTemporaryScreenCompatibleDC::~ScopedTemporaryScreenCompatibleDC() {
}

}  // namespace gfx
