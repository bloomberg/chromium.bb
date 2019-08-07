// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_NOTIFICATION_DATA_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_NOTIFICATION_DATA_CONVERSIONS_H_

#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/platform/modules/notifications/web_notification_data.h"

namespace blink {

// Converts blink::PlatformNotificationData to Blink WebNotificationData.
BLINK_PLATFORM_EXPORT WebNotificationData
ToWebNotificationData(const PlatformNotificationData& platform_data);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_NOTIFICATION_DATA_CONVERSIONS_H_
