// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "ui/message_center/message_center_export.h"

namespace content {
class RenderViewHost;
}

namespace message_center {

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class MESSAGE_CENTER_EXPORT NotificationDelegate
    : public base::RefCountedThreadSafe<NotificationDelegate> {
 public:
  // To be called when the desktop notification is actually shown.
  virtual void Display() = 0;

  // To be called when the desktop notification cannot be shown due to an
  // error.
  virtual void Error() = 0;

  // To be called when the desktop notification is closed.  If closed by a
  // user explicitly (as opposed to timeout/script), |by_user| should be true.
  virtual void Close(bool by_user) = 0;

  // Returns true if the delegate can handle click event.
  virtual bool HasClickedListener();

  // To be called when a desktop notification is clicked.
  virtual void Click() = 0;

  // To be called when the user clicks a button in a notification. TODO(miket):
  // consider providing default implementations of the pure virtuals of this
  // interface, to avoid pinging so many OWNERs each time we enhance it.
  virtual void ButtonClick(int button_index);

 protected:
  virtual ~NotificationDelegate() {}

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

}  //  namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_
