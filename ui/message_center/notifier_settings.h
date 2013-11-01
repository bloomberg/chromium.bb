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

  // Constructor for APPLICATION and SYNCED_NOTIFICATION_SERVICE type.
  NotifierId(NotifierType type, const std::string& id);

  // Constructor for WEB_PAGE type.
  explicit NotifierId(const GURL& url);

  // Constructor for system component types. The type should be positive.
  explicit NotifierId(int type);

  // The default constructor which doesn't specify the notifier. Used for tests.
  NotifierId();

  bool operator==(const NotifierId& other) const;

  NotifierType type;

  // The identifier of the app notifier. Empty if it's not APPLICATION or
  // SYNCED_NOTIFICATION_SERVICE.
  std::string id;

  // The URL pattern of the notifer.
  GURL url;

  // The type of system component notifier, usually used in ash. -1 if it's not
  // the system component. See also: ash/system/system_notifier.h
  int system_component_type;
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

struct MESSAGE_CENTER_EXPORT NotifierGroup {
  NotifierGroup(const gfx::Image& icon,
                const string16& name,
                const string16& login_info,
                size_t index);
  ~NotifierGroup();

  // Icon of a notifier group.
  const gfx::Image icon;

  // Display name of a notifier group.
  const string16 name;

  // More display information about the notifier group.
  string16 login_info;

  // Unique identifier for the notifier group so that they can be selected in
  // the UI.
  const size_t index;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifierGroup);
};

// An observer class implemented by the view of the NotifierSettings to get
// notified when the controller has changed data.
class MESSAGE_CENTER_EXPORT NotifierSettingsObserver {
 public:
  // Called when an icon in the controller has been updated.
  virtual void UpdateIconImage(const NotifierId& notifier_id,
                               const gfx::Image& icon) = 0;

  // Called when any change happens to the set of notifier groups.
  virtual void NotifierGroupChanged() = 0;
};

// A class used by NotifierSettingsView to integrate with a setting system
// for the clients of this module.
class MESSAGE_CENTER_EXPORT NotifierSettingsProvider {
 public:
  virtual ~NotifierSettingsProvider() {};

  // Sets the delegate.
  virtual void AddObserver(NotifierSettingsObserver* observer) = 0;
  virtual void RemoveObserver(NotifierSettingsObserver* observer) = 0;

  // Returns the number of notifier groups available.
  virtual size_t GetNotifierGroupCount() const = 0;

  // Requests the model for a particular notifier group.
  virtual const message_center::NotifierGroup& GetNotifierGroupAt(
      size_t index) const = 0;

  // Returns true if the notifier group at |index| is active.
  virtual bool IsNotifierGroupActiveAt(size_t index) const = 0;

  // Informs the settings provider that further requests to GetNotifierList
  // should return notifiers for the specified notifier group.
  virtual void SwitchToNotifierGroup(size_t index) = 0;

  // Requests the currently active notifier group.
  virtual const message_center::NotifierGroup& GetActiveNotifierGroup()
      const = 0;

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
