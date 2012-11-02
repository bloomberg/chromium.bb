// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NOTIFICATIONS_NOTIFICATION_TYPES_H_
#define UI_NOTIFICATIONS_NOTIFICATION_TYPES_H_

#include "ui/base/ui_export.h"

namespace ui {

namespace notifications {

// TODO(miket): these are temporary field names that will be replaced very
// shortly with real names. See
// chrome/browser/extensions/api/notification/notification_api.cc for more
// context.
UI_EXPORT extern const char kExtraFieldKey[];
UI_EXPORT extern const char kSecondExtraFieldKey[];

enum NotificationType {
  NOTIFICATION_TYPE_SIMPLE,
  NOTIFICATION_TYPE_BASE_FORMAT,
};

}  // namespace notifications

}  // namespace ui

#endif // UI_NOTIFICATIONS_NOTIFICATION_TYPES_H_
