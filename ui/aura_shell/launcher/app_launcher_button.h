// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_APP_LAUNCHER_BUTTON_H_
#define UI_AURA_SHELL_LAUNCHER_APP_LAUNCHER_BUTTON_H_
#pragma once

#include "ui/views/controls/button/image_button.h"

namespace aura_shell {
namespace internal {

class LauncherButtonHost;

// Button used for items on the launcher corresponding to an app window.
class AppLauncherButton : public views::ImageButton {
 public:
  AppLauncherButton(views::ButtonListener* listener,
                    LauncherButtonHost* host);
  virtual ~AppLauncherButton();

  // Sets the image to display for this entry.
  void SetAppImage(const SkBitmap& image);

 protected:
  // View overrides:
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;

 private:
  LauncherButtonHost* host_;

  DISALLOW_COPY_AND_ASSIGN(AppLauncherButton);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_APP_LAUNCHER_BUTTON_H_
