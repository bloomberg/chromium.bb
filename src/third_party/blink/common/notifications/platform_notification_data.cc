// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

namespace blink {

PlatformNotificationData::PlatformNotificationData()
    : direction(mojom::NotificationDirection::LEFT_TO_RIGHT) {}

PlatformNotificationData::PlatformNotificationData(
    const PlatformNotificationData& other) {
  *this = other;
}

PlatformNotificationData& PlatformNotificationData::operator=(
    const PlatformNotificationData& other) {
  if (&other == this)
    return *this;

  title = other.title;
  direction = other.direction;
  lang = other.lang;
  body = other.body;
  tag = other.tag;
  image = other.image;
  icon = other.icon;
  badge = other.badge;
  vibration_pattern = other.vibration_pattern;
  timestamp = other.timestamp;
  renotify = other.renotify;
  silent = other.silent;
  require_interaction = other.require_interaction;
  data = other.data;
  for (auto& action : other.actions)
    actions.push_back(action.Clone());
  show_trigger_timestamp = other.show_trigger_timestamp;

  return *this;
}

PlatformNotificationData::~PlatformNotificationData() = default;

}  // namespace blink
