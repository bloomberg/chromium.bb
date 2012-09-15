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
class InputMethodDelegate;
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
  virtual void ShowMaximizedWithBounds(const gfx::Rect& restored_bounds) = 0;

  virtual bool IsVisible() const = 0;

  virtual void SetSize(const gfx::Size& size) = 0;
  virtual void CenterWindow(const gfx::Size& size) = 0;
  virtual void GetWindowPlacement(gfx::Rect* bounds,
                                  ui::WindowShowState* show_state) const = 0;
  virtual gfx::Rect GetWindowBoundsInScreen() const = 0;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const = 0;
  virtual gfx::Rect GetRestoredBounds() const = 0;

  virtual void Activate() = 0;
  virtual void Deactivate() = 0;
  virtual bool IsActive() const = 0;
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;
  virtual bool IsMaximized() const = 0;
  virtual bool IsMinimized() const = 0;

  virtual void SetAlwaysOnTop(bool always_on_top) = 0;

  virtual InputMethod* CreateInputMethod() = 0;
  virtual internal::InputMethodDelegate* GetInputMethodDelegate() = 0;

  virtual void SetWindowTitle(const string16& title) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_H_
