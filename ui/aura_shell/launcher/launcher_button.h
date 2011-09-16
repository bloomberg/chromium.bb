// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_BUTTON_H_
#define UI_AURA_SHELL_LAUNCHER_BUTTON_H_
#pragma once

#include "views/controls/button/image_button.h"

namespace aura_shell {
namespace internal {

class LauncherButton : public views::ImageButton {
 public:
  explicit LauncherButton(views::ButtonListener* listener);
  virtual ~LauncherButton();

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherButton);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_BUTTON_H_
