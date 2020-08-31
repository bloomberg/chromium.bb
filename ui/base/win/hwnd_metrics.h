// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_METRICS_H_
#define UI_BASE_WIN_HWND_METRICS_H_

#include <windows.h>

#include "ui/base/ui_base_export.h"

namespace ui {

// The size, in pixels, of the non-client frame around a window on |monitor|.
UI_BASE_EXPORT int GetFrameThickness(HMONITOR monitor);

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_METRICS_H_
