// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_
#define UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_

#include <string>

#include "base/macros.h"
#include "ui/views/controls/native/native_view_host.h"

namespace exo {
class NotificationSurface;
}

namespace arc {

class ArcCustomNotificationView : public views::NativeViewHost {
 public:
  explicit ArcCustomNotificationView(exo::NotificationSurface* surface);
  ~ArcCustomNotificationView() override;

 private:
  // views::NativeViewHost
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  exo::NotificationSurface* const surface_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomNotificationView);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_VIEW_H_
