// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include "app/drag_drop_types.h"
#include "app/gfx/canvas.h"
#include "app/gfx/path.h"
#include "base/string_util.h"
#include "views/accessibility/view_accessibility_wrapper.h"
#include "views/border.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace views {

ViewAccessibilityWrapper* View::GetViewAccessibilityWrapper() {
  if (accessibility_.get() == NULL) {
    accessibility_.reset(new ViewAccessibilityWrapper(this));
  }
  return accessibility_.get();
}

void View::Focus() {
  // Set the native focus to the root view window so it receives the keyboard
  // messages.
  FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->FocusNativeView(GetRootView()->GetWidget()->GetNativeView());
}

int View::GetHorizontalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CXDRAG) / 2;
  return threshold;
}

int View::GetVerticalDragThreshold() {
  static int threshold = -1;
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CYDRAG) / 2;
  return threshold;
}

}  // namespace views
