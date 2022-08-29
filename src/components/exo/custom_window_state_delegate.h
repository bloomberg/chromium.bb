// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CUSTOM_WINDOW_STATE_DELEGATE_H_
#define COMPONENTS_EXO_CUSTOM_WINDOW_STATE_DELEGATE_H_

#include "ash/wm/window_state_delegate.h"

namespace exo {
class ShellSurface;

// CustomWindowStateDelegate for ShellSurface to override default fullscreen
// behavior and optionally provide a resize presentation time recorder for
// ShellSurface.
class CustomWindowStateDelegate : public ash::WindowStateDelegate {
 public:
  CustomWindowStateDelegate();
  explicit CustomWindowStateDelegate(ShellSurface* shell_surface);

  CustomWindowStateDelegate(const CustomWindowStateDelegate&) = delete;
  CustomWindowStateDelegate& operator=(const CustomWindowStateDelegate&) =
      delete;

  ~CustomWindowStateDelegate() override;

  // ash::WindowStateDelegate:
  bool ToggleFullscreen(ash::WindowState* window_state) override;
  void ToggleLockedFullscreen(ash::WindowState* window_state) override;
  std::unique_ptr<ash::PresentationTimeRecorder> OnDragStarted(
      int component) override;

 private:
  ShellSurface* const shell_surface_;
};

}  //  namespace exo

#endif  // COMPONENTS_EXO_CUSTOM_WINDOW_STATE_DELEGATE_H_
