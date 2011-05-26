// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/desktop/desktop_window_root_view.h"

#include "views/desktop/desktop_window.h"
#include "views/widget/native_widget_view.h"
#include "views/window/window.h"

namespace views {
namespace desktop {

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowRootView, public:

DesktopWindowRootView::DesktopWindowRootView(DesktopWindow* desktop_window,
                                             Window* window)
    : internal::RootView(window),
      desktop_window_(desktop_window) {
}

DesktopWindowRootView::~DesktopWindowRootView() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowRootView, internal::RootView overrides:

bool DesktopWindowRootView::OnMousePressed(const MouseEvent& event) {
  View* target = GetEventHandlerForPoint(event.location());
  if (target->GetClassName() == internal::NativeWidgetView::kViewClassName) {
    internal::NativeWidgetView* native_widget_view =
        static_cast<internal::NativeWidgetView*>(target);
    desktop_window_->ActivateWidget(native_widget_view->GetAssociatedWidget());
  } else {
    desktop_window_->ActivateWidget(NULL);
  }
  return RootView::OnMousePressed(event);
}

}  // namespace desktop
}  // namespace views
