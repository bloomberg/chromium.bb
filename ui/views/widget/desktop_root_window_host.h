// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_H_
#define UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_H_

#include "ui/base/ui_base_types.h"
#include "ui/views/widget/widget.h"

namespace aura {
class RootWindowHost;
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
namespace internal {
class NativeWidgetDelegate;
}

class DesktopRootWindowHost {
 public:
  virtual ~DesktopRootWindowHost() {}

  static DesktopRootWindowHost* Create(
      internal::NativeWidgetDelegate* native_widget_delegate,
      const gfx::Rect& initial_bounds);

  virtual void Init(aura::Window* content_window,
                    const Widget::InitParams& params) = 0;

  virtual void Close() = 0;
  virtual void CloseNow() = 0;

  virtual aura::RootWindowHost* AsRootWindowHost() = 0;

  virtual void ShowWindowWithState(ui::WindowShowState show_state) = 0;

  virtual bool IsVisible() const = 0;

  virtual gfx::Rect GetClientAreaBoundsInScreen() const = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_H_
