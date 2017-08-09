// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center.h"

#include "base/command_line.h"
#include "base/observer_list.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_switches.h"

namespace message_center {

//------------------------------------------------------------------------------

namespace {
static MessageCenter* g_message_center = NULL;
}

// static
void MessageCenter::Initialize() {
  DCHECK(g_message_center == NULL);
  g_message_center = new MessageCenterImpl();
}

// static
MessageCenter* MessageCenter::Get() {
  DCHECK(g_message_center);
  return g_message_center;
}

// static
void MessageCenter::Shutdown() {
  DCHECK(g_message_center);
  delete g_message_center;
  g_message_center = NULL;
}

// static
bool MessageCenter::IsNewStyleNotificationEnabled() {
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

MessageCenter::MessageCenter() {
}

MessageCenter::~MessageCenter() {
}

}  // namespace message_center
