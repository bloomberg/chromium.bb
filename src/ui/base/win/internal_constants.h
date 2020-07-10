// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_INTERNAL_CONSTANTS_H_
#define UI_BASE_WIN_INTERNAL_CONSTANTS_H_

#include "ui/base/ui_base_export.h"

namespace ui {

// This window property if set on the window does not activate the window for a
// touch based WM_MOUSEACTIVATE message.
UI_BASE_EXPORT extern const wchar_t kIgnoreTouchMouseActivateForWindow[];

// This property is put on an HWND so the compositor output knows to treat it
// as transparent and draw to it using WS_EX_LAYERED (if using the software
// compositor).
UI_BASE_EXPORT extern const wchar_t kWindowTranslucent[];

}  // namespace ui

#endif  // UI_BASE_WIN_INTERNAL_CONSTANTS_H_


