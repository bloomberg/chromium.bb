// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_
#define UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_export.h"
#include "url/gurl.h"

class MessageCenterNotificationsTest;
class MessageCenterTrayBridgeTest;

namespace ash {
class WebNotificationTrayTest;
}

namespace message_center {
namespace test {
class MessagePopupCollectionTest;
}

class MessageCenterNotificationManagerTest;
class Notification;
class NotifierSettingsDelegate;
class NotifierSettingsProvider;

// Brings up the settings dialog and returns a weak reference to the delegate,
// which is typically the view. If the dialog already exists, it is brought to
// the front, otherwise it is created.
MESSAGE_CENTER_EXPORT NotifierSettingsDelegate* ShowSettings(
    NotifierSettingsProvider* provider,
    gfx::NativeView context);

// A struct that identifies the source of notifications. For example, a web page
// might send multiple notifications but they'd all have the same NotifierId.
// TODO(estade): rename to Notifier.
struct MESSAGE_CENTER_EXPORT NotifierId {
  // This enum is being used for histogram reporting and the elements should not
  // be re-ordered.
  enum NotifierType : int {
    APPLICATION = 0,
    ARC_APPLICATION = 1,
    WEB_PAGE = 2,
    SYSTEM_COMPONENT = 3,
    SIZE,
  };

  // Constructor for non WEB_PAGE type.
  NotifierId(NotifierType type, const std::string& id);

  // Constructor for WEB_PAGE type.
  explicit NotifierId(const GURL& url);

  NotifierId(const NotifierId& other);

  bool operator==(const NotifierId& other) const;
  // Allows NotifierId to be used as a key in std::map.
  bool operator<(const NotifierId& other) const;

  NotifierType type;

  // The identifier of the app notifier. Empty if it's WEB_PAGE.
  std::string id;

  // The URL pattern of the notifer.
  GURL url;

  // The identifier of the profile where the notification is created. This is
  // used for ChromeOS multi-profile support and can be empty.
  std::string profile_id;

 private:
  friend class MessageCenterNotificationManagerTest;
  friend class MessageCenterTrayTest;
  friend class Notification;
  friend class NotificationControllerTest;
  friend class PopupCollectionTest;
  friend class TrayViewControllerTest;
  friend class ::MessageCenterNotificationsTest;
  friend class ::MessageCenterTrayBridgeTest;
  friend class ash::WebNotificationTrayTest;
  friend class test::MessagePopupCollectionTest;
  FRIEND_TEST_ALL_PREFIXES(PopupControllerTest, Creation);
  FRIEND_TEST_ALL_PREFIXES(NotificationListTest, UnreadCountNoNegative);
  FRIEND_TEST_ALL_PREFIXES(NotificationListTest, TestHasNotificationOfType);

  // The default constructor which doesn't specify the notifier. Used for tests.
  NotifierId();
};

// A struct to hold UI information about notifiers. The data is used by
// NotifierSettingsView. TODO(estade): rename to NotifierUiData.
struct MESSAGE_CENTER_EXPORT Notifier {
  Notifier(const NotifierId& notifier_id,
           const base::string16& name,
           bool enabled);
  ~Notifier();

  NotifierId notifier_id;

  // The human-readable name of the notifier such like the extension name.
  // It can be empty.
  base::string16 name;

  // True if the source is allowed to send notifications. True is default.
  bool enabled;

  // The icon image of the notifier. The extension icon or favicon.
  gfx::Image icon;

 private:
  DISALLOW_COPY_AND_ASSIGN(Notifier);
};

// An observer class implemented by the view of the NotifierSettings to get
// notified when the controller has changed data.
class MESSAGE_CENTER_EXPORT NotifierSettingsObserver {
 public:
  // Called when an icon in the controller has been updated.
  virtual void UpdateIconImage(const NotifierId& notifier_id,
                               const gfx::Image& icon){};

  // Called when a notifier is enabled or disabled.
  virtual void NotifierEnabledChanged(const NotifierId& notifier_id,
                                      bool enabled){};
};

// A class used by NotifierSettingsView to integrate with a setting system
// for the clients of this module.
class MESSAGE_CENTER_EXPORT NotifierSettingsProvider {
 public:
  virtual ~NotifierSettingsProvider() {}

  // Sets the delegate.
  virtual void AddObserver(NotifierSettingsObserver* observer) = 0;
  virtual void RemoveObserver(NotifierSettingsObserver* observer) = 0;

  // Provides the current notifier list in |notifiers|.
  virtual void GetNotifierList(
      std::vector<std::unique_ptr<Notifier>>* notifiers) = 0;

  // Called when the |enabled| for the given notifier has been changed by user
  // operation.
  virtual void SetNotifierEnabled(const NotifierId& notifier_id,
                                  bool enabled) = 0;

  // Called when the settings window is closed.
  virtual void OnNotifierSettingsClosing() = 0;

  // Called to determine if a particular notifier can respond to a request for
  // more information.
  virtual bool NotifierHasAdvancedSettings(const NotifierId& notifier_id)
      const = 0;

  // Called upon request for more information about a particular notifier.
  virtual void OnNotifierAdvancedSettingsRequested(
      const NotifierId& notifier_id,
      const std::string* notification_id) = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_
