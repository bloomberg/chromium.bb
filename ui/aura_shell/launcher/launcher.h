// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_LAUNCHER_H_
#define UI_AURA_SHELL_LAUNCHER_LAUNCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class ToplevelWindowContainer;
}

namespace views {
class Widget;
}

namespace aura_shell {

class LauncherModel;

class AURA_SHELL_EXPORT Launcher : public aura::WindowObserver {
 public:
  explicit Launcher(aura::ToplevelWindowContainer* window_container);
  ~Launcher();

  LauncherModel* model() { return model_.get(); }
  views::Widget* widget() { return widget_; }

 private:
  // WindowObserver overrides:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;

  scoped_ptr<LauncherModel> model_;

  // Widget hosting the view.
  views::Widget* widget_;

  aura::ToplevelWindowContainer* window_container_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_LAUNCHER_H_
