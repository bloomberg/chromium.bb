// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_SWITCHES_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_SWITCHES_H_

#include "ui/message_center/message_center_export.h"

namespace message_center {
namespace switches {

// Disables the new design of message center, which shows each notification as a
// card.
// TODO(mukai): Remove this flag when we don't need to provide both of designs
// anymore (i.e. the new design becomes default and no one complains about it).
// Note that some classes should be removed and renamed as the result of
// removing this class.
// Affected class list:
//  - WebNotificationButtonView2: remove '2' suffix and replace the old one.
//  - WebNotificationButtonViewBase: merge into WebNotificationButtonView.
MESSAGE_CENTER_EXPORT extern const char kDisableNewMessageCenterBubble[];

}  // namespace switches
}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_CONSTANTS_H_
