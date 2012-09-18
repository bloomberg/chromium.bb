// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/viewer/viewer_host_win.h"

#include "ui/aura/client/stacking_client.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_capture_client.h"
#include "ui/aura/single_display_manager.h"

class ViewerStackingClient : public aura::client::StackingClient {
 public:
  explicit ViewerStackingClient(aura::RootWindow* root_window)
      : root_window_(root_window) {
    aura::client::SetStackingClient(this);
  }

  virtual ~ViewerStackingClient() {
    aura::client::SetStackingClient(NULL);
  }

  // Overridden from aura::client::StackingClient:
  virtual aura::Window* GetDefaultParent(aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    return root_window_;
  }

 private:
  aura::RootWindow* root_window_;

  scoped_ptr<aura::shared::RootWindowCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(ViewerStackingClient);
};

ViewerHostWin::ViewerHostWin() {
  aura::DisplayManager::set_use_fullscreen_host_window(true);
  aura::Env::GetInstance()->SetDisplayManager(new aura::SingleDisplayManager);
  root_window_.reset(aura::DisplayManager::CreateRootWindowForPrimaryDisplay());
  root_window_capture_client_.reset(
      new aura::shared::RootWindowCaptureClient(root_window_.get()));
  focus_manager_.reset(new aura::FocusManager);
  root_window_->set_focus_manager(focus_manager_.get());
  stacking_client_.reset(new ViewerStackingClient(root_window_.get()));
  root_window_->ShowRootWindow();
}

ViewerHostWin::~ViewerHostWin() {
}
