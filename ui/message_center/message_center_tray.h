// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_TRAY_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_TRAY_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/notifier_settings.h"

namespace ui {
class MenuModel;
}

namespace message_center {

class MessageCenter;

// Class that observes a MessageCenter. Manages the popup and message center
// bubbles. Tells the MessageCenterTrayHost when the tray is changed, as well
// as when bubbles are shown and hidden.
class MESSAGE_CENTER_EXPORT MessageCenterTray : public MessageCenterObserver {
 public:
  MessageCenterTray(MessageCenterTrayDelegate* delegate,
                    message_center::MessageCenter* message_center);
  ~MessageCenterTray() override;

  // Shows or updates the message center bubble and hides the popup bubble. Set
  // |show_by_click| to true if bubble is shown by mouse or gesture click.
  // Returns whether the message center is visible after the call, whether or
  // not it was visible before.
  bool ShowMessageCenterBubble(bool show_by_click);

  // Hides the message center if visible and returns whether the message center
  // was visible before.
  bool HideMessageCenterBubble();

  // Marks the message center as "not visible" (this method will not hide the
  // message center).
  void MarkMessageCenterHidden();

  // Causes an update if the popup bubble is already shown.
  void ShowPopupBubble();

  // Returns whether the popup was visible before.
  bool HidePopupBubble();

  // Toggles the visibility of the settings view in the message center bubble.
  void ShowNotifierSettingsBubble();

  // Creates a model for the context menu for a notification card.
  std::unique_ptr<ui::MenuModel> CreateNotificationMenuModel(
      const Notification& notification);

  bool message_center_visible() { return message_center_visible_; }
  bool popups_visible() { return popups_visible_; }
  MessageCenterTrayDelegate* delegate() { return delegate_; }
  const message_center::MessageCenter* message_center() const {
    return message_center_;
  }
  message_center::MessageCenter* message_center() { return message_center_; }

  // Overridden from MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationClicked(const std::string& notification_id) override;
  void OnNotificationButtonClicked(const std::string& notification_id,
                                   int button_index) override;
  void OnNotificationSettingsClicked(bool handled) override;
  void OnNotificationDisplayed(const std::string& notification_id,
                               const DisplaySource source) override;
  void OnQuietModeChanged(bool in_quiet_mode) override;
  void OnBlockingStateChanged(NotificationBlocker* blocker) override;

 private:
  void OnMessageCenterChanged();
  void NotifyMessageCenterTrayChanged();
  void HidePopupBubbleInternal();

  // |message_center_| is a weak pointer that must live longer than
  // MessageCenterTray.
  message_center::MessageCenter* message_center_;
  bool message_center_visible_;
  bool popups_visible_;
  // |delegate_| is a weak pointer that must live longer than MessageCenterTray.
  MessageCenterTrayDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterTray);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_TRAY_H_
