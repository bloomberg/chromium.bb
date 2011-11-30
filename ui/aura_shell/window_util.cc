// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/window_util.h"

#include "ui/aura/window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ui_base_types.h"

namespace aura_shell {

bool IsWindowMaximized(aura::Window* window) {
  return window->GetIntProperty(aura::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

}  // namespace aura_shell
