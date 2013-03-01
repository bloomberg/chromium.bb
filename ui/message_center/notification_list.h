// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_

#include <set>
#include <string>

#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

namespace base {
class DictionaryValue;
}

namespace message_center {

// Comparers used to auto-sort the lists of Notifications.
struct ComparePriorityTimestampSerial {
  bool operator()(Notification* n1, Notification* n2);
};

struct CompareTimestampSerial {
  bool operator()(Notification* n1, Notification* n2);
};

// A helper class to manage the list of notifications.
class MESSAGE_CENTER_EXPORT NotificationList {
 public:
  // Auto-sorted set. Matches the order in which Notifications are shown in
  // Notification Center.
  typedef std::set<Notification*, ComparePriorityTimestampSerial> Notifications;

  // Auto-sorted set used to return the Notifications to be shown as popup
  // toasts.
  typedef std::set<Notification*, CompareTimestampSerial> PopupNotifications;

  class MESSAGE_CENTER_EXPORT Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Removes notifications
    virtual void SendRemoveNotification(const std::string& id,
                                        bool by_user) = 0;
    virtual void SendRemoveAllNotifications(bool by_user) = 0;

    // Disables notifications
    virtual void DisableNotificationByExtension(const std::string& id) = 0;
    virtual void DisableNotificationByUrl(const std::string& id) = 0;

    // Requests the Delegate to show the settings page.
    virtual void ShowNotificationSettings(const std::string& id) = 0;

    // Requests the Delegate to show the settings dialog.
    virtual void ShowNotificationSettingsDialog(gfx::NativeView context) = 0;

    // Called when a notification is clicked on.
    virtual void OnNotificationClicked(const std::string& id) = 0;

    // Called when the quiet mode status has been changed.
    virtual void OnQuietModeChanged(bool quiet_mode) = 0;

    // Called when a button in a notification is clicked. |button_index|
    // indicates which button was clicked, zero-indexed (button one is 0,
    // button two is 1).
    virtual void OnButtonClicked(const std::string& id, int button_index) = 0;

    // Returns the list of notifications to display.
    virtual NotificationList* GetNotificationList() = 0;
  };

  explicit NotificationList(Delegate* delegate);
  virtual ~NotificationList();

  // Affects whether or not a message has been "read".
  void SetMessageCenterVisible(bool visible);

  void AddNotification(NotificationType type,
                       const std::string& id,
                       const string16& title,
                       const string16& message,
                       const string16& display_source,
                       const std::string& extension_id,
                       const base::DictionaryValue* optional_fields);

  void UpdateNotificationMessage(const std::string& old_id,
                                 const std::string& new_id,
                                 const string16& title,
                                 const string16& message,
                                 const base::DictionaryValue* optional_fields);

  void RemoveNotification(const std::string& id);

  void RemoveAllNotifications();

  void SendRemoveNotificationsBySource(const std::string& id);

  void SendRemoveNotificationsByExtension(const std::string& id);

  // Returns true if the notification exists and was updated.
  bool SetNotificationIcon(const std::string& notification_id,
                           const gfx::ImageSkia& image);

  // Returns true if the notification exists and was updated.
  bool SetNotificationImage(const std::string& notification_id,
                            const gfx::ImageSkia& image);

  // Returns true if the notification and button exist and were updated.
  bool SetNotificationButtonIcon(const std::string& notification_id,
                                 int button_index,
                                 const gfx::ImageSkia& image);

  bool HasNotification(const std::string& id);

  // Returns false if the first notification has been shown as a popup (which
  // means that all notifications have been shown).
  bool HasPopupNotifications();

  // Returns the recent notifications of the priority higher then LOW,
  // that have not been shown as a popup. kMaxVisiblePopupNotifications are
  // used to limit the number of notifications for the DEFAULT priority.
  // The returned list is sorted by timestamp, newer first.
  PopupNotifications GetPopupNotifications();

  // Marks the popups for the |priority| as shown.
  void MarkPopupsAsShown(int priority);

  // Marks a specific popup item as shown. Set |mark_notification_as_read| to
  // true in case marking the notification as read too.
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read);

  bool quiet_mode() const { return quiet_mode_; }

  // Sets the current quiet mode status to |quiet_mode|. The new status is not
  // expired.
  void SetQuietMode(bool quiet_mode);

  // Sets the current quiet mode to true. The quiet mode will expire in the
  // specified time-delta from now.
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in);

  // Returns all notifications, in a (priority-timestamp) order. Suitable for
  // rendering notifications in a NotificationCenter.
  const Notifications& GetNotifications();
  size_t NotificationCount() const;
  size_t unread_count() const { return unread_count_; }

  static const size_t kMaxVisiblePopupNotifications;
  static const size_t kMaxVisibleMessageCenterNotifications;

 private:
  // Iterates through the list and returns the first notification matching |id|.
  Notifications::iterator GetNotification(const std::string& id);

  void EraseNotification(Notifications::iterator iter);

  void PushNotification(scoped_ptr<Notification> notification);

  // Sets the current quiet mode status to |quiet_mode|.
  void SetQuietModeInternal(bool quiet_mode);

  Delegate* delegate_;
  Notifications notifications_;
  bool message_center_visible_;
  size_t unread_count_;
  bool quiet_mode_;
  scoped_ptr<base::OneShotTimer<NotificationList> > quiet_mode_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationList);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
