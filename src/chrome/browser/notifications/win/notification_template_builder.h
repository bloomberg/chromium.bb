// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_TEMPLATE_BUILDER_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_TEMPLATE_BUILDER_H_

#include "base/strings/string16.h"

class NotificationLaunchId;

namespace message_center {
class Notification;
}  // namespace message_center

class NotificationImageRetainer;

// The Notification Toast element name in the toast XML.
extern const char kNotificationToastElement[];

// The Notification Launch attribute name in the toast XML.
extern const char kNotificationLaunchAttribute[];

// Builds XML-based notification template for displaying a given |notification|
// in the Windows Action Center.
base::string16 BuildNotificationTemplate(
    NotificationImageRetainer* image_retainer,
    const NotificationLaunchId& launch_id,
    const message_center::Notification& notification);

// Sets the label of the context menu for testing. The caller owns |label| and
// is responsible for resetting the override back to nullptr.
void SetContextMenuLabelForTesting(const char* label);

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_NOTIFICATION_TEMPLATE_BUILDER_H_
