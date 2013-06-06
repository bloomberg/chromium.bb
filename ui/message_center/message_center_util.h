// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_UTIL_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_UTIL_H_

#include "ui/message_center/message_center_export.h"

namespace message_center {

MESSAGE_CENTER_EXPORT bool IsRichNotificationEnabled();

// If Rich Notificaitons are enabled by default on a platform, run the
// corresponding tests on that platform.
#if defined(OS_WIN) || defined(USE_AURA) || defined(OS_MACOSX)
#define RUN_MESSAGE_CENTER_TESTS 1
#endif

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_UTIL_H_
