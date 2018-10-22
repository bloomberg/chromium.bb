// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_DATA_CONVERSIONS_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_DATA_CONVERSIONS_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

struct NotificationDatabaseData;

// Parses the serialized notification data |input| into a new object, |output|.
// Returns whether the serialized |input| could be deserialized successfully.
CONTENT_EXPORT bool DeserializeNotificationDatabaseData(
    const std::string& input,
    NotificationDatabaseData* output);

// Serializes the contents of |input| into the string |output|. Returns whether
// the notification data could be serialized successfully.
CONTENT_EXPORT bool SerializeNotificationDatabaseData(
    const NotificationDatabaseData& input,
    std::string* output);

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_DATABASE_DATA_CONVERSIONS_H_
