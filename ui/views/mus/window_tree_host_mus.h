// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_

#include "base/macros.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/views/mus/mus_export.h"

class SkBitmap;

namespace mojo {
namespace shell {
namespace mojom {
class Shell;
}
}
}

namespace mus {
class Window;
}

namespace views {

class InputMethodMUS;
class NativeWidgetMus;
class PlatformWindowMus;

class VIEWS_MUS_EXPORT WindowTreeHostMus : public aura::WindowTreeHostPlatform {
 public:
  WindowTreeHostMus(mojo::shell::mojom::Shell* shell,
                    NativeWidgetMus* native_widget_,
                    mus::Window* window);
  ~WindowTreeHostMus() override;

  PlatformWindowMus* platform_window();
  ui::PlatformWindowState show_state() const { return show_state_; }

 private:
  // aura::WindowTreeHostPlatform:
  void DispatchEvent(ui::Event* event) override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnActivationChanged(bool active) override;
  void OnCloseRequest() override;

  NativeWidgetMus* native_widget_;
  scoped_ptr<InputMethodMUS> input_method_;
  ui::PlatformWindowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
