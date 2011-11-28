// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
#define UI_AURA_SHELL_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
#pragma once

#include "ui/aura_shell/launcher/launcher_types.h"
#include "ui/views/controls/button/custom_button.h"

namespace aura_shell {
namespace internal {

class LauncherButtonHost;

// Button used for items on the launcher corresponding to tabbed windows.
class TabbedLauncherButton : public views::CustomButton {
 public:
  TabbedLauncherButton(views::ButtonListener* listener,
                       LauncherButtonHost* host);
  virtual ~TabbedLauncherButton();

  // Sets the images to display for this entry.
  void SetImages(const LauncherTabbedImages& images);

  // View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 protected:
  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;

 private:
  LauncherTabbedImages images_;

  LauncherButtonHost* host_;

  // Background images. Which one is chosen depends upon how many images are
  // provided.
  static SkBitmap* bg_image_1_;
  static SkBitmap* bg_image_2_;
  static SkBitmap* bg_image_3_;

  DISALLOW_COPY_AND_ASSIGN(TabbedLauncherButton);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_TABBED_LAUNCHER_BUTTON_H_
