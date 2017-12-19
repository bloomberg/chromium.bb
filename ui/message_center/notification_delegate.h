// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class MESSAGE_CENTER_EXPORT NotificationDelegate
    : public base::RefCountedThreadSafe<NotificationDelegate> {
 public:
  // To be called when the desktop notification is closed.  If closed by a
  // user explicitly (as opposed to timeout/script), |by_user| should be true.
  virtual void Close(bool by_user);

  // To be called when a desktop notification is clicked.
  virtual void Click();

  // To be called when the user clicks a button in a notification.
  virtual void ButtonClick(int button_index);

  // To be called when the user types a reply to a notification.
  // TODO(crbug.com/599859) Support this feature in the message center -
  // currently it is only supported on Android.
  virtual void ButtonClickWithReply(int button_index,
                                    const base::string16& reply);

  // To be called when the user clicks the settings button in a notification
  // which has a CUSTOM settings button action.
  virtual void SettingsClick();

  // Called when the user attempts to disable the notification.
  virtual void DisableNotification();

 protected:
  virtual ~NotificationDelegate() {}

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

// A simple notification delegate which invokes the passed closure when the body
// or a button is clicked.
class MESSAGE_CENTER_EXPORT HandleNotificationClickDelegate
    : public NotificationDelegate {
 public:
  // The parameter is the index of the button that was clicked, or nullopt if
  // the body was clicked.
  using ButtonClickCallback =
      base::RepeatingCallback<void(base::Optional<int>)>;

  // Creates a delegate that handles clicks on a button or on the body.
  explicit HandleNotificationClickDelegate(const ButtonClickCallback& callback);

  // Creates a delegate that only handles clicks on the body of the
  // notification.
  explicit HandleNotificationClickDelegate(
      const base::RepeatingClosure& closure);

  // message_center::NotificationDelegate overrides:
  void Click() override;
  void ButtonClick(int button_index) override;

 protected:
  ~HandleNotificationClickDelegate() override;

 private:
  ButtonClickCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(HandleNotificationClickDelegate);
};

}  //  namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_
