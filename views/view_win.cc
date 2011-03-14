// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include <atlcomcli.h>
//  Necessary to define oleacc GUID's used in window_win.cc.
#include <initguid.h>
#include <oleacc.h>

#include "base/string_util.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "views/accessibility/native_view_accessibility_win.h"
#include "views/border.h"
#include "views/views_delegate.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/widget/widget_win.h"

namespace views {

NativeViewAccessibilityWin* View::GetNativeViewAccessibilityWin() {
  if (!native_view_accessibility_win_.get())
    native_view_accessibility_win_.swap(
        NativeViewAccessibilityWin::Create(this));
  return native_view_accessibility_win_.get();
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
