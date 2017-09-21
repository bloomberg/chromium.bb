// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/public/cpp/message_center_switches.h"

#include "base/command_line.h"

namespace message_center {

bool IsNewStyleNotificationEnabled() {
// For Chrome OS, the default is Enabled.
// For other platforms, the default is Disabled.
#if defined(OS_CHROMEOS)
  // Returns true if not explicitly disabled.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableMessageCenterNewStyleNotification);
#else
  // Returns true if explicitly enabled.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMessageCenterNewStyleNotification);
#endif
}

namespace switches {

// Flag to enable or disable new-style notification. This flag will be removed
// once the feature gets stable.
const char kEnableMessageCenterNewStyleNotification[] =
    "enabled-new-style-notification";
const char kDisableMessageCenterNewStyleNotification[] =
    "disabled-new-style-notification";

}  // namespace switches

}  // namespace message_center
