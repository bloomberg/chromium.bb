// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_GROUP_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_GROUP_VIEW_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/message_center/views/message_view.h"

namespace message_center {

class BoundedLabel;
class MessageCenter;
class NotificationButton;

// View that displays a placeholder representing several notifications and a
// button to expand it into a set of individual notifications.
class GroupView : public MessageView, public MessageViewController {
 public:
  // The group view currently shows the latest notificaiton w/o buttons
  // and a single button like "N more from XYZ".
  // Each GroupView has a notifier_id which it uses to report clicks and other
  // user actions to Client.
  GroupView(MessageCenterController* controller,
            const NotifierId& notifier_id,
            const Notification& last_notification,
            const gfx::ImageSkia& group_icon,
            int group_size);

  virtual ~GroupView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from MessageView:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from MessageViewController:
  virtual void ClickOnNotification(const std::string& notification_id) OVERRIDE;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) OVERRIDE;
  virtual void DisableNotificationsFromThisSource(
      const NotifierId& notifier_id) OVERRIDE;
  virtual void ShowNotifierSettingsBubble() OVERRIDE;

 private:
  MessageCenterController* controller_;  // Weak, controls lifetime of views.
  NotifierId notifier_id_;
  string16 display_source_;
  gfx::ImageSkia group_icon_;
  int group_size_;
  std::string last_notification_id_;

  // Weak references to GroupView descendants owned by their parents.
  views::View* background_view_;
  views::View* top_view_;
  views::View* bottom_view_;
  BoundedLabel* title_view_;
  BoundedLabel* message_view_;
  BoundedLabel* context_message_view_;
  views::View* icon_view_;
  NotificationButton* more_button_;

  DISALLOW_COPY_AND_ASSIGN(GroupView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_GROUP_VIEW_H_
