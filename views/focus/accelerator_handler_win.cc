// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

#include "views/event.h"
#include "views/focus/focus_manager.h"

namespace views {

AcceleratorHandler::AcceleratorHandler() {
}

bool AcceleratorHandler::Dispatch(const MSG& msg) {
  bool process_message = true;

  if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) {
    FocusManager* focus_manager =
        FocusManager::GetFocusManagerForNativeView(msg.hwnd);
    if (focus_manager) {
      switch (msg.message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
          KeyEvent event(Event::ET_KEY_PRESSED,
                         msg.wParam,
                         msg.lParam & 0xFFFF,
                         (msg.lParam & 0xFFFF0000) >> 16);
          process_message = focus_manager->OnKeyEvent(event);
          break;
        }
      }
    }
  }

  if (process_message) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return true;
}

}  // namespace views
