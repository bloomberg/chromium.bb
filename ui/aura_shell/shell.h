// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_H_
#define UI_AURA_SHELL_SHELL_H_
#pragma once

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class EventFilter;
class Window;
}
namespace gfx {
class Rect;
}

namespace aura_shell {

class Launcher;
class ShellDelegate;

namespace internal {
class DragDropController;
class ShadowController;
class ShelfLayoutController;
class WorkspaceController;
}

// Shell is a singleton object that presents the Shell API and implements the
// Desktop's delegate interface.
class AURA_SHELL_EXPORT Shell {
 public:
  // Upon creation, the Shell sets itself as the Desktop's delegate, which takes
  // ownership of the Shell.

  // A shell must be explicitly created so that it can call |Init()| with the
  // delegate set. |delegate| can be NULL (if not required for initialization).
  static Shell* CreateInstance(ShellDelegate* delegate);

  // Should never be called before |CreateInstance()|.
  static Shell* GetInstance();

  static void DeleteInstanceForTesting();

  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  // Adds or removes |filter| from the DesktopEventFilter.
  void AddDesktopEventFilter(aura::EventFilter* filter);
  void RemoveDesktopEventFilter(aura::EventFilter* filter);

  // Toggles between overview mode and normal mode.
  void ToggleOverview();

  ShellDelegate* delegate() { return delegate_.get(); }
  Launcher* launcher() { return launcher_.get(); }

  // Made available for tests.
  internal::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  explicit Shell(ShellDelegate* delegate);
  virtual ~Shell();

  void Init();

  // Enables WorkspaceManager.
  void EnableWorkspaceManager();

  static Shell* instance_;

  std::vector<WindowAndBoundsPair> to_restore_;

  base::WeakPtrFactory<Shell> method_factory_;

  scoped_ptr<ShellDelegate> delegate_;

  scoped_ptr<Launcher> launcher_;

  scoped_ptr<internal::DragDropController> drag_drop_controller_;
  scoped_ptr<internal::WorkspaceController> workspace_controller_;
  scoped_ptr<internal::ShelfLayoutController> shelf_layout_controller_;
  scoped_ptr<internal::ShadowController> shadow_controller_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_H_
