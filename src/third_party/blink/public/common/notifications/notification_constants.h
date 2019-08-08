// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NOTIFICATIONS_NOTIFICATION_CONSTANTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NOTIFICATIONS_NOTIFICATION_CONSTANTS_H_

#include "base/time/time.h"

namespace blink {

// Maximum allowed time delta into the future for show triggers. Allow a bit
// more than a year to account for leap years and seconds.
constexpr base::TimeDelta kMaxNotificationShowTriggerDelay =
    base::TimeDelta::FromDays(367);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NOTIFICATIONS_NOTIFICATION_CONSTANTS_H_
