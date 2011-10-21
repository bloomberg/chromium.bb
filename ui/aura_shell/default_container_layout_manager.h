// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace aura_shell {
namespace internal {

// LayoutManager for the default window container.
class DefaultContainerLayoutManager : public aura::LayoutManager {
 public:
  explicit DefaultContainerLayoutManager(aura::Window* owner);
  virtual ~DefaultContainerLayoutManager();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAdded(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void CalculateBoundsForChild(aura::Window* child,
                                       gfx::Rect* requested_bounds) OVERRIDE;

 private:
  aura::Window* owner_;

  DISALLOW_COPY_AND_ASSIGN(DefaultContainerLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DEFAULT_CONTAINER_LAYOUT_MANAGER_H_
