// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

MESSAGE_CENTER_EXPORT extern const int kNotificationIconSize;
MESSAGE_CENTER_EXPORT extern const int kNotificationPreferredImageSize;
MESSAGE_CENTER_EXPORT extern const int kNotificationButtonIconSize;
MESSAGE_CENTER_EXPORT extern const int kSettingsIconSize;

// The square size of the control buttons (close, expand, etc.).
extern const int kControlButtonSize;

// The amount of spacing between the icon and the title/message.
extern const int kIconToTextPadding;

// The amount of vertical space between text elements.
extern const int kTextTopPadding;

extern const int kNotificationWidth;

extern const int kNotificationMaximumImageHeight;
extern const size_t kNotificationMaximumItems;

extern const int kAutocloseHighPriorityDelaySeconds;
extern const int kAutocloseDefaultDelaySeconds;

extern const int kMarginBetweenItems;

// The background color of the notification view.
extern const SkColor kBackgroundColor;

// The background color used behind icons that are not large enough to fill
// the bounds of the icon view.
extern const SkColor kLegacyIconBackgroundColor;

// The color of normal text elements in a notification.
extern const SkColor kRegularTextColor;

// The font size of the title.
extern const int kTitleFontSize;

// The font size of the message.
extern const int kMessageFontSize;

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
