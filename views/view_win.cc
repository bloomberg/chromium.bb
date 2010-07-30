// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include "app/drag_drop_types.h"
#include "base/string_util.h"
#include "gfx/canvas.h"
#include "gfx/path.h"
#include "views/accessibility/view_accessibility.h"
#include "views/accessibility/view_accessibility_wrapper.h"
#include "views/border.h"
#include "views/views_delegate.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/widget/widget_win.h"

namespace views {

// static
int View::GetDoubleClickTimeMS() {
  return ::GetDoubleClickTime();
}

// static
int View::GetMenuShowDelay() {
  static DWORD delay = 0;
  if (!delay && !SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &delay, 0))
    delay = View::kShowFolderDropMenuDelay;
  return delay;
}

void View::NotifyAccessibilityEvent(AccessibilityTypes::Event event_type) {
  NotifyAccessibilityEvent(event_type, true);
}

void View::NotifyAccessibilityEvent(AccessibilityTypes::Event event_type,
    bool send_native_event) {
  // Send the notification to the delegate.
  if (ViewsDelegate::views_delegate)
    ViewsDelegate::views_delegate->NotifyAccessibilityEvent(this, event_type);

  // Now call the Windows-specific method to notify MSAA clients of this
  // event.  The widget gives us a temporary unique child ID to associate
  // with this view so that clients can call get_accChild in ViewAccessibility
  // to retrieve the IAccessible associated with this view.
  if (send_native_event) {
    WidgetWin* view_widget = static_cast<WidgetWin*>(GetWidget());
    int child_id = view_widget->AddAccessibilityViewEvent(this);
    ::NotifyWinEvent(ViewAccessibility::MSAAEvent(event_type),
        view_widget->GetNativeView(), OBJID_CLIENT, child_id);
  }
}

ViewAccessibilityWrapper* View::GetViewAccessibilityWrapper() {
  if (accessibility_.get() == NULL) {
    accessibility_.reset(new ViewAccessibilityWrapper(this));
  }
  return accessibility_.get();
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
