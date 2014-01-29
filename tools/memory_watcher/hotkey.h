// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MEMORY_WATCHER_HOTKEY_H_
#define TOOLS_MEMORY_WATCHER_HOTKEY_H_

#include "ui/gfx/rect.h"
#include "ui/gfx/win/msg_util.h"
#include "ui/gfx/win/window_impl.h"

// HotKey handler.
// Programs wishing to register a hotkey can use this.
class HotKeyHandler : public gfx::WindowImpl {
 public:
  HotKeyHandler(UINT modifiers, UINT vk)
    : modifiers_(modifiers),
      vkey_(vk) {
    Start();
  }
  ~HotKeyHandler() { Stop(); }

  CR_BEGIN_MSG_MAP_EX(HotKeyHandler)
    CR_MSG_WM_HOTKEY(OnHotKey)
  CR_END_MSG_MAP()

 private:
  static const int hotkey_id = 0x0000baba;

  bool Start() {
    set_window_style(WS_POPUP);
    Init(NULL, gfx::Rect());
    return RegisterHotKey(hwnd(), hotkey_id, modifiers_, vkey_) == TRUE;
  }

  void Stop() {
    UnregisterHotKey(hwnd(), hotkey_id);
    DestroyWindow(hwnd());
  }

  // Handle the registered Hotkey being pressed.
  virtual void OnHotKey(UINT /*uMsg*/,
                        WPARAM /*wParam*/,
                        LPARAM /*lParam*/) = 0;

  UINT modifiers_;
  UINT vkey_;
};

#endif  // TOOLS_MEMORY_WATCHER_HOTKEY_H_
