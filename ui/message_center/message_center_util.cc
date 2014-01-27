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
#if defined(OS_CHROMEOS)
  return true;
#elif !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(USE_AURA)
  return false;
#endif

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRichNotifications)) {
    return false;
  }
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableRichNotifications)) {
    return true;
  }

  return true;
}

bool IsExperimentalNotificationUIEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalNotificationUI);
}

MessageCenterShowState GetMessageCenterShowState() {
#if defined(OS_MACOSX)
  std::string tray_behavior =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNotificationCenterTrayBehavior);
  if (tray_behavior == "never")
    return MESSAGE_CENTER_SHOW_NEVER;
  if (tray_behavior == "always")
    return MESSAGE_CENTER_SHOW_ALWAYS;
  if (tray_behavior == "unread")
    return MESSAGE_CENTER_SHOW_UNREAD;
  return MESSAGE_CENTER_SHOW_AFTER_FIRST;
#elif defined(OS_CHROMEOS)
  return MESSAGE_CENTER_SHOW_UNREAD;
#else  // defined(OS_WIN) || defined(OS_LINUX)
  return MESSAGE_CENTER_SHOW_AFTER_FIRST;
#endif
}

}  // namespace message_center
