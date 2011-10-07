// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_H_
#define UI_AURA_SHELL_SHELL_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/desktop_delegate.h"

namespace aura {
class Window;
}

namespace aura_shell {

// Shell is a singleton object that presents the Shell API and implements the
// Desktop's delegate interface.
class Shell : public aura::DesktopDelegate {
 public:
  // Upon creation, the Shell sets itself as the Desktop's delegate, which takes
  // ownership of the Shell.
  Shell();
  virtual ~Shell();

  void Init();

 private:
  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  // Overridden from aura::DesktopDelegate:
  virtual void AddChildToDefaultParent(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetTopmostWindowToActivate(
      aura::Window* ignore) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_H_
