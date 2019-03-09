// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/context_menu_controller.h"

#include "base/auto_reset.h"

namespace views {

void ContextMenuController::ShowContextMenuForView(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Use a boolean flag to early-exit out of re-entrant behavior.
  if (is_opening_)
    return;
  base::AutoReset<bool> auto_reset_is_opening(&is_opening_, true);

  ShowContextMenuForViewImpl(source, point, source_type);
}

}  // namespace views
