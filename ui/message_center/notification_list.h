// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_

#include <list>
#include <map>
#include <string>

#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/notifications/notification_types.h"

namespace base {
class DictionaryValue;
}

namespace message_center {

// A helper class to manage the list of notifications.
class MESSAGE_CENTER_EXPORT NotificationList {
 public:
  struct MESSAGE_CENTER_EXPORT NotificationItem {
    string16 title;
    string16 message;
    NotificationItem(string16 title, string16 message)
        : title(title),
          message(message) {}
  };

  struct MESSAGE_CENTER_EXPORT Notification {
    Notification();
    virtual ~Notification();

    ui::notifications::NotificationType type;
    std::string id;
    string16 title;
    string16 message;
    string16 display_source;
    std::string extension_id;

    // Begin unpacked values from optional_fields
    int priority;
    base::Time timestamp;
    int unread_count;
    string16 button_one_title;
    string16 button_two_title;
    string16 expanded_message;
    std::vector<NotificationItem> items;
    // End unpacked values

    // Images fetched asynchronously
    gfx::ImageSkia primary_icon;
    gfx::ImageSkia secondary_icon;
    gfx::ImageSkia image;

    bool is_read;  // True if this has been seen in the message center
    bool shown_as_popup;  // True if this has been shown as a popup notification
  };

  typedef std::list<Notification> Notifications;

  class MESSAGE_CENTER_EXPORT Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Removes notifications
    virtual void SendRemoveNotification(const std::string& id) = 0;
    virtual void SendRemoveAllNotifications() = 0;

    // Disables notifications
    virtual void DisableNotificationByExtension(const std::string& id) = 0;
    virtual void DisableNotificationByUrl(const std::string& id) = 0;

    // Requests the Delegate to the settings dialog.
    virtual void ShowNotificationSettings(const std::string& id) = 0;

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

  void AddNotification(ui::notifications::NotificationType type,
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

  // Returns true if the notification was removed.
  bool RemoveNotification(const std::string& id);

  void RemoveAllNotifications();

  void SendRemoveNotificationsBySource(const std::string& id);

  void SendRemoveNotificationsByExtension(const std::string& id);

  // Returns true if the notification exists and was updated.
  bool SetNotificationPrimaryIcon(const std::string& id,
                                  const gfx::ImageSkia& image);

  // Returns true if the notification exists and was updated.
  bool SetNotificationSecondaryIcon(const std::string& id,
                                    const gfx::ImageSkia& image);

  // Returns true if the notification exists and was updated.
  bool SetNotificationImage(const std::string& id, const gfx::ImageSkia& image);

  bool HasNotification(const std::string& id);

  // Returns false if the first notification has been shown as a popup (which
  // means that all notifications have been shown).
  bool HasPopupNotifications();

  // Modifies |notifications| to contain the |kMaxVisiblePopupNotifications|
  // least recent notifications that have not been shown as a popup.
  void GetPopupNotifications(Notifications* notifications);

  // Marks the popups for the |priority| as shown.
  void MarkPopupsAsShown(int priority);

  bool quiet_mode() const { return quiet_mode_; }

  // Sets the current quiet mode status to |quiet_mode|. The new status is not
  // expired.
  void SetQuietMode(bool quiet_mode);

  // Sets the current quiet mode to true. The quiet mode will expire in the
  // specified time-delta from now.
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in);

  void GetNotifications(Notifications* notifications) const;
  size_t NotificationCount() const;
  size_t unread_count() const { return unread_count_; }

  static const size_t kMaxVisiblePopupNotifications;
  static const size_t kMaxVisibleMessageCenterNotifications;

 private:
  typedef std::map<int, Notifications> NotificationMap;

  // Iterates through the list and stores the first notification matching |id|
  // (should always be unique) to |iter|. Returns true if it's found.
  bool GetNotification(const std::string& id, Notifications::iterator* iter);

  void EraseNotification(Notifications::iterator iter);

  void PushNotification(Notification& notification);

  // Returns the recent notifications of the |priority| that have not been shown
  // as a popup. kMaxVisiblePopupNotifications are used to limit the number of
  // notifications for the default priority.
  void GetPopupIterators(int priority,
                         Notifications::iterator& first,
                         Notifications::iterator& last);

  // Given a dictionary of optional notification fields (or NULL), unpacks all
  // recognized values into the given Notification struct. We assume prior
  // proper initialization of |notification| fields that correspond to
  // |optional_fields|.
  void UnpackOptionalFields(const base::DictionaryValue* optional_fields,
                            Notification* notification);

  // Sets the current quiet mode status to |quiet_mode|.
  void SetQuietModeInternal(bool quiet_mode);

  Delegate* delegate_;
  NotificationMap notifications_;
  bool message_center_visible_;
  size_t unread_count_;
  bool quiet_mode_;
  scoped_ptr<base::OneShotTimer<NotificationList> > quiet_mode_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationList);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_NOTIFICATION_LIST_H_
