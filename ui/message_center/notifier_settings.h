// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_
#define UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_export.h"
#include "url/gurl.h"

namespace message_center {

class NotifierSettingsDelegate;
class NotifierSettingsProvider;

// Brings up the settings dialog and returns a weak reference to the delegate,
// which is typically the view. If the dialog already exists, it is brought to
// the front, otherwise it is created.
MESSAGE_CENTER_EXPORT NotifierSettingsDelegate* ShowSettings(
    NotifierSettingsProvider* provider,
    gfx::NativeView context);

// The struct to distinguish the notifiers.
struct MESSAGE_CENTER_EXPORT NotifierId {
  enum NotifierType {
    APPLICATION,
    WEB_PAGE,
    SYSTEM_COMPONENT,
    SYNCED_NOTIFICATION_SERVICE,
  };

  enum SystemComponentNotifierType {
    NONE,
    SCREENSHOT,
  };

  // Constructor for APPLICATION and SYNCED_NOTIFICATION_SERVICE type.
  NotifierId(NotifierType type, const std::string& id);

  // Constructor for WEB_PAGE type.
  explicit NotifierId(const GURL& url);

  // Constructor for SYSTEM_COMPONENT type.
  explicit NotifierId(SystemComponentNotifierType type);

  bool operator==(const NotifierId& other) const;

  NotifierType type;

  // The identifier of the app notifier. Empty if it's not APPLICATION or
  // SYNCED_NOTIFICATION_SERVICE.
  std::string id;

  // The URL pattern of the notifer.
  GURL url;

  // The type of system component notifier.
  SystemComponentNotifierType system_component_type;
};

// The struct to hold the information of notifiers. The information will be
// used by NotifierSettingsView.
struct MESSAGE_CENTER_EXPORT Notifier {
  Notifier(const NotifierId& notifier_id, const string16& name, bool enabled);
  ~Notifier();

  NotifierId notifier_id;

  // The human-readable name of the notifier such like the extension name.
  // It can be empty.
  string16 name;

  // True if the source is allowed to send notifications. True is default.
  bool enabled;

  // The icon image of the notifier. The extension icon or favicon.
  gfx::Image icon;

 private:
  DISALLOW_COPY_AND_ASSIGN(Notifier);
};

MESSAGE_CENTER_EXPORT std::string ToString(
    NotifierId::SystemComponentNotifierType type);
MESSAGE_CENTER_EXPORT NotifierId::SystemComponentNotifierType
    ParseSystemComponentName(const std::string& name);

// An observer class implemented by the view of the NotifierSettings to get
// notified when the controller has changed data.
class MESSAGE_CENTER_EXPORT NotifierSettingsObserver {
 public:
  // Called when an icon in the controller has been updated.
  virtual void UpdateIconImage(const NotifierId& notifier_id,
                               const gfx::Image& icon) = 0;
};

// A class used by NotifierSettingsView to integrate with a setting system
// for the clients of this module.
class MESSAGE_CENTER_EXPORT NotifierSettingsProvider {
 public:
  // Sets the delegate.
  virtual void AddObserver(NotifierSettingsObserver* observer) = 0;
  virtual void RemoveObserver(NotifierSettingsObserver* observer) = 0;

  // Collects the current notifier list and fills to |notifiers|. Caller takes
  // the ownership of the elements of |notifiers|.
  virtual void GetNotifierList(std::vector<Notifier*>* notifiers) = 0;

  // Called when the |enabled| for the |notifier| has been changed by user
  // operation.
  virtual void SetNotifierEnabled(const Notifier& notifier, bool enabled) = 0;

  // Called when the settings window is closed.
  virtual void OnNotifierSettingsClosing() = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFIER_SETTINGS_H_
