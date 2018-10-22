// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_MESSAGE_CENTER_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_MESSAGE_CENTER_BRIDGE_H_

#import <AppKit/AppKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/popups_only_ui_controller.h"
#include "components/prefs/pref_member.h"
#include "ui/message_center/message_center.h"

@class MCPopupCollection;

namespace message_center {
class MessageCenter;
}  // namespace message_center

// MessageCenterBridge is the owner of all the Cocoa UI objects for the
// message_center. It bridges C++ notifications from the PoupsOnlyUiController
// to the various UI objects.
class MessageCenterBridge : public PopupsOnlyUiController::Delegate {
 public:
  explicit MessageCenterBridge(message_center::MessageCenter* message_center);
  ~MessageCenterBridge() override;

  // PopupsUiController::Delegate:
  void ShowPopups() override;
  void HidePopups() override;

 private:
  friend class MessageCenterBridgeTest;

  // The global, singleton message center model object. Weak.
  message_center::MessageCenter* const message_center_;

  // Obj-C controller for the on-screen popup notifications.
  base::scoped_nsobject<MCPopupCollection> popup_collection_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_MESSAGE_CENTER_BRIDGE_H_
