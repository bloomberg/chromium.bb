// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"

class TrayViewControllerTest;

namespace base {
class DictionaryValue;
}

// Interface to manage the NotificationList. The client (e.g. Chrome) calls
// [Add|Remove|Update]Notification to create and update notifications in the
// list. It also sends those changes to its observers when a notification
// is shown, closed, or clicked on.
// It can also implement Delegate to ask platform-dependent features like
// disabling extensions or opening settings.

namespace message_center {

class MessageCenterObserver;
class NotificationList;
class NotifierSettingsDelegate;
class NotifierSettingsProvider;

class MESSAGE_CENTER_EXPORT MessageCenter {
 public:
  // Creates the global message center object.
  static void Initialize();

  // Returns the global message center object. Initialize must be called first.
  static MessageCenter* Get();

  // Destroys the global message_center object.
  static void Shutdown();

  class MESSAGE_CENTER_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Request to disable the extension associated with |notification_id|.
    virtual void DisableExtension(const std::string& notification_id) = 0;

    // Request to disable notifications from the source of |notification_id|.
    virtual void DisableNotificationsFromSource(
        const std::string& notification_id) = 0;

    // Request to show the notification settings (|notification_id| is used
    // to identify the requesting browser context).
    virtual void ShowSettings(const std::string& notification_id) = 0;
  };

  // Called to set the delegate.  Generally called only once, except in tests.
  // Changing the delegate does not affect notifications in its
  // NotificationList.
  virtual void SetDelegate(Delegate* delegate) = 0;

  // Management of the observer list.
  virtual void AddObserver(MessageCenterObserver* observer) = 0;
  virtual void RemoveObserver(MessageCenterObserver* observer) = 0;

  // Queries of current notification list status.
  virtual size_t NotificationCount() const = 0;
  virtual size_t UnreadNotificationCount() const = 0;
  virtual bool HasPopupNotifications() const = 0;
  virtual bool HasNotification(const std::string& id) = 0;
  virtual bool IsQuietMode() const = 0;
  virtual bool HasClickedListener(const std::string& id) = 0;

  // Getters of the current notifications.
  virtual const NotificationList::Notifications& GetNotifications() = 0;
  // Gets all notifications being shown as popups.
  virtual NotificationList::PopupNotifications GetPopupNotifications() = 0;

  // Basic operations of notification: add/remove/update.

  // Adds a new notification.
  virtual void AddNotification(scoped_ptr<Notification> notification) = 0;

  // Updates an existing notification with id = old_id and set its id to new_id.
  virtual void UpdateNotification(
      const std::string& old_id,
      scoped_ptr<Notification> new_notification) = 0;

  // Removes an existing notification.
  virtual void RemoveNotification(const std::string& id, bool by_user) = 0;
  virtual void RemoveAllNotifications(bool by_user) = 0;

  // Sets the icon image. Icon appears at the top-left of the notification.
  virtual void SetNotificationIcon(const std::string& notification_id,
                                   const gfx::Image& image) = 0;

  // Sets the large image for the notifications of type == TYPE_IMAGE. Specified
  // image will appear below of the notification.
  virtual void SetNotificationImage(const std::string& notification_id,
                                    const gfx::Image& image) = 0;

  // Sets the image for the icon of the specific action button.
  virtual void SetNotificationButtonIcon(const std::string& notification_id,
                                         int button_index,
                                         const gfx::Image& image) = 0;

  // Operations happening especially from GUIs: click, expand, disable,
  // and settings.
  // Searches through the notifications and disables any that match the
  // extension id given.
  virtual void DisableNotificationsByExtension(const std::string& id) = 0;

  // Disables all notifications that match the given url by querying the
  // delegate and also by matching display_source.
  // TODO(dewittj): Is display_source matching necessary?
  virtual void DisableNotificationsByUrl(const std::string& url) = 0;

  // TODO(mukai): settings can be in another class?
  // Shows the settings for a web notification (profile is identified by the
  // given notification id).
  virtual void ShowNotificationSettings(const std::string& id) = 0;

  // Reformat a notification to show its entire text content.
  virtual void ExpandNotification(const std::string& id) = 0;

  // This should be called by UI classes when a notification is clicked to
  // trigger the notification's delegate callback and also update the message
  // center observers.
  virtual void ClickOnNotification(const std::string& id) = 0;

  // This should be called by UI classes when a notification button is clicked
  // to trigger the notification's delegate callback and also update the message
  // center observers.
  virtual void ClickOnNotificationButton(const std::string& id,
                                         int button_index) = 0;

  // This should be called by UI classes after a visible notification popup
  // closes, indicating that the notification has been shown to the user.
  // |mark_notification_as_read|, if false, will unset the read bit on a
  // notification, increasing the unread count of the center.
  virtual void MarkSinglePopupAsShown(const std::string& id,
                                      bool mark_notification_as_read) = 0;

  // This should be called by UI classes when a notification is first displayed
  // to the user, in order to decrement the unread_count for the tray, and to
  // notify observers that the notification is visible.
  virtual void DisplayedNotification(const std::string& id) = 0;

  // Setter/getter of notifier settings provider. This will be a weak reference.
  // This should be set at the initialization process. The getter may return
  // NULL for tests.
  virtual void SetNotifierSettingsProvider(
      NotifierSettingsProvider* provider) = 0;
  virtual NotifierSettingsProvider* GetNotifierSettingsProvider() = 0;

  // This can be called to change the quiet mode state (without a timeout).
  virtual void SetQuietMode(bool in_quiet_mode) = 0;

  // Temporarily enables quiet mode for |expires_in| time.
  virtual void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) = 0;

  // Informs the notification list whether the message center is visible.
  // This affects whether or not a message has been "read".
  virtual void SetMessageCenterVisible(bool visible) = 0;

  // Allows querying the visibility of the center.
  virtual bool IsMessageCenterVisible() = 0;

  // UI classes should call this when there is cause to leave popups visible for
  // longer than the default (for example, when the mouse hovers over a popup).
  virtual void PausePopupTimers() = 0;

  // UI classes should call this when the popup timers should restart (for
  // example, after the mouse leaves the popup.)
  virtual void RestartPopupTimers() = 0;

 protected:
  friend class ::TrayViewControllerTest;
  virtual void DisableTimersForTest() = 0;

  MessageCenter();
  virtual ~MessageCenter();

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenter);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
