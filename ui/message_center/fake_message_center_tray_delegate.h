// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_TRAY_DELEGATE_H_
#define UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_TRAY_DELEGATE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/message_center/message_center_tray_delegate.h"

namespace message_center {

class MessageCenter;
class MessageCenterTray;

// A message center tray delegate which does nothing.
class FakeMessageCenterTrayDelegate : public MessageCenterTrayDelegate {
 public:
  FakeMessageCenterTrayDelegate(MessageCenter* message_center,
                                base::Closure quit_closure);
  virtual ~FakeMessageCenterTrayDelegate();

  bool displayed_first_run_balloon() const {
    return displayed_first_run_balloon_;
  }

  // Overridden from MessageCenterTrayDelegate:
  virtual void OnMessageCenterTrayChanged() override;
  virtual bool ShowPopups() override;
  virtual void HidePopups() override;
  virtual bool ShowMessageCenter() override;
  virtual void HideMessageCenter() override;
  virtual bool ShowNotifierSettings() override;
  virtual bool IsContextMenuEnabled() const override;
  virtual MessageCenterTray* GetMessageCenterTray() override;
  virtual void DisplayFirstRunBalloon() override;

 private:
  scoped_ptr<MessageCenterTray> tray_;
  base::Closure quit_closure_;
  bool displayed_first_run_balloon_;

  DISALLOW_COPY_AND_ASSIGN(FakeMessageCenterTrayDelegate);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_TRAY_DELEGATE_H_
