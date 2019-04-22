// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/switchable_windows.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"

namespace ash {
namespace wm {

const int kSwitchableWindowContainerIds[] = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_AlwaysOnTopContainer};

const size_t kSwitchableWindowContainerIdsLength =
    base::size(kSwitchableWindowContainerIds);

bool IsSwitchableContainer(const aura::Window* window) {
  if (!window)
    return false;
  const int shell_window_id = window->id();
  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
    if (shell_window_id == kSwitchableWindowContainerIds[i])
      return true;
  }
  return false;
}

}  // namespace wm
}  // namespace ash
