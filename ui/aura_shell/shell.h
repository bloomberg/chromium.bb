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
#include "ui/aura/desktop_delegate.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "views/controls/button/button.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}
namespace views {
class ImageButton;
}

namespace aura_shell {

class LauncherModel;
class ShellDelegate;

// Shell is a singleton object that presents the Shell API and implements the
// Desktop's delegate interface.
class AURA_SHELL_EXPORT Shell : public aura::DesktopDelegate,
                                public views::ButtonListener {
 public:
  // Upon creation, the Shell sets itself as the Desktop's delegate, which takes
  // ownership of the Shell.
  Shell();
  virtual ~Shell();

  static Shell* GetInstance();

  void Init();

  // Sets the delegate. Shell owns its delegate.
  void SetDelegate(ShellDelegate* delegate);

  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  void TileWindows();
  void RestoreTiledWindows();

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  void InitLauncherModel();

  // Overridden from aura::DesktopDelegate:
  virtual void AddChildToDefaultParent(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetTopmostWindowToActivate(
      aura::Window* ignore) const OVERRIDE;

  // Overriden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  static Shell* instance_;

  std::vector<WindowAndBoundsPair> to_restore_;

  base::WeakPtrFactory<Shell> method_factory_;

  scoped_ptr<LauncherModel> launcher_model_;
  views::ImageButton* new_browser_button_;
  views::ImageButton* show_apps_button_;

  scoped_ptr<ShellDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELL_H_
