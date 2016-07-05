// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_
#define UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/arc/notification/arc_custom_notification_item.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/native/native_view_host.h"

namespace exo {
class NotificationSurface;
}

namespace views {
class ImageButton;
class Widget;
}

namespace arc {

class ArcCustomNotificationView : public views::NativeViewHost,
                                  public views::ButtonListener,
                                  public ArcCustomNotificationItem::Observer {
 public:
  ArcCustomNotificationView(ArcCustomNotificationItem* item,
                            exo::NotificationSurface* surface);
  ~ArcCustomNotificationView() override;

 private:
  void CreateFloatingCloseButton();

  // views::NativeViewHost
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  void Layout() override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ArcCustomNotificationItem::Observer
  void OnItemDestroying() override;
  void OnItemPinnedChanged() override;
  void OnItemNotificationSurfaceRemoved() override;

  ArcCustomNotificationItem* item_;
  exo::NotificationSurface* surface_;

  // A close button on top of NotificationSurface. Needed because the
  // aura::Window of NotificationSurface is added after hosting widget's
  // RootView thus standard notification close button is always below
  // it.
  std::unique_ptr<views::Widget> floating_close_button_widget_;

  views::ImageButton* floating_close_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomNotificationView);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_
