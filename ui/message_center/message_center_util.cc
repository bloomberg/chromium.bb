// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_util.h"

#include "base/command_line.h"
#include "ui/message_center/message_center_switches.h"

namespace message_center {

// TODO(dimich): remove this function and the kEnableRichNotifications flag
// when a time period in Canary indicates the new notifications are acceptable
// for default behavior.
bool IsRichNotificationEnabled() {
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(USE_ASH)
  return false;
#endif

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRichNotifications))
    return false;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRichNotifications))
    return true;

// Not enabled by default on Mac yet.
#if defined(OS_MACOSX)
  return false;
#endif

  return true;
}

}  // namespace message_center
