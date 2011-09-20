// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DESKTOP_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_DESKTOP_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"

namespace aura {
class Window;
}
namespace views {
class Widget;
}

namespace aura_shell {
namespace internal {

class AURA_SHELL_EXPORT DesktopLayoutManager : public aura::LayoutManager {
 public:
  explicit DesktopLayoutManager(aura::Window* owner);
  virtual ~DesktopLayoutManager();

  void set_toplevel_window_container(aura::Window* toplevel_window_container) {
    toplevel_window_container_ = toplevel_window_container;
  }

  void set_background_widget(views::Widget* background_widget) {
    background_widget_ = background_widget;
  }

  void set_launcher_widget(views::Widget* launcher_widget) {
    launcher_widget_ = launcher_widget;
  }

  void set_status_area_widget(views::Widget* status_area_widget) {
    status_area_widget_ = status_area_widget;
  }

 private:
  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;

  aura::Window* owner_;
  aura::Window* toplevel_window_container_;
  views::Widget* background_widget_;
  views::Widget* launcher_widget_;
  views::Widget* status_area_widget_;

  DISALLOW_COPY_AND_ASSIGN(DesktopLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DESKTOP_LAYOUT_MANAGER_H_
