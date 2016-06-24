// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_view.h"

#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "ui/views/widget/widget.h"

namespace arc {

ArcCustomNotificationView::ArcCustomNotificationView(
    exo::NotificationSurface* surface)
    : surface_(surface) {}

ArcCustomNotificationView::~ArcCustomNotificationView() {}

void ArcCustomNotificationView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  views::Widget* widget = GetWidget();

  // Bail if native_view() has attached to a different widget.
  if (widget && native_view() &&
      views::Widget::GetTopLevelWidgetForNativeView(native_view()) != widget) {
    return;
  }

  views::NativeViewHost::ViewHierarchyChanged(details);

  if (!widget || !details.is_add)
    return;

  SetPreferredSize(surface_->GetSize());
  Attach(surface_->window());
}

}  // namespace arc
