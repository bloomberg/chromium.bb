// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_RUBBERBAND_WINDOWS_H_
#define UI_BASE_WIN_RUBBERBAND_WINDOWS_H_

#include "base/macros.h"
#include "ui/base/ui_base_export.h"

#include <windows.h>
#include <memory>

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class RubberbandWindow;

class UI_BASE_EXPORT RubberbandOutline {
 public:
  RubberbandOutline();
  ~RubberbandOutline();

  void SetRect(HWND parent, RECT rect);

private:
  std::unique_ptr<RubberbandWindow> windows_[4];

  DISALLOW_COPY_AND_ASSIGN(RubberbandOutline);
};

}  // namespace ui

#endif  // UI_BASE_WIN_WINDOW_IMPL_H_
