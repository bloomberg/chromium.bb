// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_LAUNCHER_H_
#define UI_AURA_SHELL_LAUNCHER_LAUNCHER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace aura_shell {

class LauncherModel;

class AURA_SHELL_EXPORT Launcher : public aura::WindowObserver {
 public:
  explicit Launcher(aura::Window* window_container);
  ~Launcher();

  LauncherModel* model() { return model_.get(); }
  views::Widget* widget() { return widget_; }

 private:
  typedef std::map<aura::Window*, bool> WindowMap;

  // If necessary asks the delegate if an entry should be created in the
  // launcher for |window|. This only asks the delegate once for a window.
  void MaybeAdd(aura::Window* window);

  // WindowObserver overrides:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visibile) OVERRIDE;

  scoped_ptr<LauncherModel> model_;

  // Widget hosting the view.
  views::Widget* widget_;

  aura::Window* window_container_;

  // The set of windows we know about. The boolean indicates whether we've asked
  // the delegate if the window should added to the launcher.
  WindowMap known_windows_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_LAUNCHER_H_
