// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/message_center_bridge.h"

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#import "ui/message_center/cocoa/popup_collection.h"
#include "ui/message_center/message_center.h"

// static
std::unique_ptr<PopupsOnlyUiController::Delegate>
PopupsOnlyUiController::CreateDelegate() {
  return std::make_unique<MessageCenterBridge>(
      message_center::MessageCenter::Get());
}

MessageCenterBridge::MessageCenterBridge(
    message_center::MessageCenter* message_center)
    : message_center_(message_center) {}

MessageCenterBridge::~MessageCenterBridge() {}

void MessageCenterBridge::ShowPopups() {
  popup_collection_.reset(
      [[MCPopupCollection alloc] initWithMessageCenter:message_center_]);
}

void MessageCenterBridge::HidePopups() {
  popup_collection_.reset();
}
