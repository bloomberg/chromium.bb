// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_FOREGROUND_HELPER_H_
#define UI_BASE_WIN_FOREGROUND_HELPER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/win/window_impl.h"

#include <windows.h>

namespace ui {

// Helper class for moving a window to the foreground.
// Windows XP and later will not allow a window which is in the background to
// move to the foreground, unless requested by the current window in the
// foreground.  For automated testing, we really want some of our windows
// to be capable of moving to the foreground.
//
// This is probably leveraging a windows bug.
class UI_BASE_EXPORT ForegroundHelper : public gfx::WindowImpl {
 public:
  ForegroundHelper();
  ~ForegroundHelper() override;

  CR_BEGIN_MSG_MAP_EX(ForegroundHelper)
    CR_MSG_WM_HOTKEY(OnHotKey)
  CR_END_MSG_MAP()

  // Brings a window into the foreground.
  // Can be called from any window, even if the caller is not the
  // foreground window.
  static HRESULT SetForeground(HWND window);

 private:
  HRESULT ForegroundHotKey(HWND window);

  // Handle the registered Hotkey being pressed.
  void OnHotKey(int id, UINT vcode, UINT modifiers);

  HWND window_;

  CR_MSG_MAP_CLASS_DECLARATIONS(ForegroundHelper)

  DISALLOW_COPY_AND_ASSIGN(ForegroundHelper);
};

}  // namespace ui

#endif  // UI_BASE_WIN_FOREGROUND_HELPER_H_
