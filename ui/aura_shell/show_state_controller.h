// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHOW_STATE_CONTROLLER_H_
#define UI_AURA_SHELL_SHOW_STATE_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace aura_shell {
namespace internal {

class WorkspaceManager;

// ShowStateController controls the window's bounds when
// the window's show state property has changed.
class ShowStateController : public aura::WindowObserver {
public:
  explicit ShowStateController(WorkspaceManager* layout_manager);
  virtual ~ShowStateController();

  // aura::WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const char* name,
                                       void* old) OVERRIDE;

 private:
  // |workspace_maanger_| is owned by |WorkspaceController|.
  WorkspaceManager* workspace_manager_;

  DISALLOW_COPY_AND_ASSIGN(ShowStateController);
};

}  // namepsace aura_shell
}  // namepsace internal

#endif  // UI_AURA_SHELL_SHOW_STATE_CONTROLLER_H_
