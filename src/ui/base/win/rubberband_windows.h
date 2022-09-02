// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_RUBBERBAND_WINDOWS_H_
#define UI_BASE_WIN_RUBBERBAND_WINDOWS_H_

#include "base/component_export.h"

#include <windows.h>
#include <memory>

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class RubberbandWindow;

class COMPONENT_EXPORT(UI_BASE) RubberbandOutline {
 public:
  RubberbandOutline();
  ~RubberbandOutline();
  RubberbandOutline(const RubberbandOutline&) = delete;
  RubberbandOutline& operator=(const RubberbandOutline&) = delete;

  void SetRect(HWND parent, RECT rect);

private:
  std::unique_ptr<RubberbandWindow> windows_[4];
};

}  // namespace ui

#endif  // UI_BASE_WIN_WINDOW_IMPL_H_
