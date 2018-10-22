// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_OVERFLOW_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_OVERFLOW_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_menu/app_menu_export.h"
#include "ash/app_menu/notification_item_view.h"

namespace message_center {
class ProportionalImageView;
}

namespace views {
class MenuSeparator;
}

namespace ash {
class NotificationOverflowImageView;

class APP_MENU_EXPORT NotificationOverflowView : public views::View {
 public:
  NotificationOverflowView();

  ~NotificationOverflowView() override;

  // Creates a copy of |image_view| and adds it as a child view, using
  // |notification_id| as an identifier.
  void AddIcon(const message_center::ProportionalImageView& image_view,
               const std::string& notification_id);

  // Removes an icon by its |notification_id|.
  void RemoveIcon(const std::string& notification_id);

  // views::View overrides:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // Whether this has notifications to show.
  bool is_empty() const { return image_views_.empty(); }

 private:
  // Removes |overflow_icon_| if it is no longer needed.
  void MaybeRemoveOverflowIcon();

  // The horizontal separator that is placed between the displayed
  // NotificationItemView and the overflow icons. Owned by the views hierarchy.
  views::MenuSeparator* separator_;

  // The list of overflow icons. Listed in right to left ordering.
  std::vector<std::unique_ptr<NotificationOverflowImageView>> image_views_;

  // The overflow icon shown when there are more than |kMaxOverflowIcons|
  // notifications.
  std::unique_ptr<message_center::ProportionalImageView> overflow_icon_;

  DISALLOW_COPY_AND_ASSIGN(NotificationOverflowView);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_OVERFLOW_VIEW_H_
