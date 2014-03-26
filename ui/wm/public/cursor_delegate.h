// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_CURSOR_DELEGATE_H_
#define UI_WM_PUBLIC_CURSOR_DELEGATE_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/wm/core/wm_core_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace wm {

class WM_CORE_EXPORT CursorDelegate {
 public:
  virtual gfx::NativeCursor GetCursorForPoint(const gfx::Point& point) = 0;

 protected:
  virtual ~CursorDelegate() {}
};

WM_CORE_EXPORT void SetCursorDelegate(aura::Window* window,
                                      CursorDelegate* delegate);
WM_CORE_EXPORT CursorDelegate* GetCursorDelegate(aura::Window* window);

}  // namespace wm

#endif  // UI_WM_PUBLIC_CURSOR_DELEGATE_H_
