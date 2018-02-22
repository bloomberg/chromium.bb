// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_DELEGATE_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_DELEGATE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "ui/message_center/public/cpp/message_center_public_export.h"

namespace message_center {

// Handles actions performed on a notification.
class MESSAGE_CENTER_PUBLIC_EXPORT NotificationObserver {
 public:
  // To be called when the desktop notification is closed.  If closed by a
  // user explicitly (as opposed to timeout/script), |by_user| should be true.
  virtual void Close(bool by_user) {}

  // To be called when a desktop notification is clicked.
  virtual void Click() {}

  // To be called when the user clicks a button in a notification.
  virtual void ButtonClick(int button_index) {}

  // To be called when the user types a reply to a notification.
  virtual void ButtonClickWithReply(int button_index,
                                    const base::string16& reply) {}

  // To be called when the user clicks the settings button in a notification
  // which has a DELEGATE settings button action.
  virtual void SettingsClick() {}

  // Called when the user attempts to disable the notification.
  virtual void DisableNotification() {}
};

// Ref counted version of NotificationObserver, required to satisfy
// message_center::Notification::delegate_.
class MESSAGE_CENTER_PUBLIC_EXPORT NotificationDelegate
    : public NotificationObserver,
      public base::RefCountedThreadSafe<NotificationDelegate> {
 protected:
  virtual ~NotificationDelegate() = default;

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

// A pass-through which converts the RefCounted requirement to a WeakPtr
// requirement. This class replaces the need for individual delegates that pass
// through to an actual controller class, and which only exist because the
// actual controller has a strong ownership model. TODO(estade): replace many
// existing NotificationDelegate overrides with an instance of this class.
class MESSAGE_CENTER_PUBLIC_EXPORT ThunkNotificationDelegate
    : public NotificationDelegate {
 public:
  explicit ThunkNotificationDelegate(base::WeakPtr<NotificationObserver> impl);

  // NotificationDelegate:
  void Close(bool by_user) override;
  void Click() override;
  void ButtonClick(int button_index) override;
  void ButtonClickWithReply(int button_index,
                            const base::string16& reply) override;
  void SettingsClick() override;
  void DisableNotification() override;

 protected:
  ~ThunkNotificationDelegate() override;

 private:
  base::WeakPtr<NotificationObserver> impl_;

  DISALLOW_COPY_AND_ASSIGN(ThunkNotificationDelegate);
};

// A simple notification delegate which invokes the passed closure when the body
// or a button is clicked.
class MESSAGE_CENTER_PUBLIC_EXPORT HandleNotificationClickDelegate
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

  // NotificationDelegate overrides:
  void Click() override;
  void ButtonClick(int button_index) override;

 protected:
  ~HandleNotificationClickDelegate() override;

 private:
  ButtonClickCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(HandleNotificationClickDelegate);
};

}  //  namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_NOTIFICATION_DELEGATE_H_
