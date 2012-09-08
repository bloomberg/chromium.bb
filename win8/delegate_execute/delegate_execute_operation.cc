// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/delegate_execute_operation.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "chrome/common/chrome_switches.h"

namespace delegate_execute {

DelegateExecuteOperation::DelegateExecuteOperation()
    : operation_type_(DELEGATE_EXECUTE),
      relaunch_flags_("") {
}

DelegateExecuteOperation::~DelegateExecuteOperation() {
}

bool DelegateExecuteOperation::Init(const CommandLine* cmd_line) {
  FilePath shortcut(cmd_line->GetSwitchValuePath(switches::kRelaunchShortcut));
  if (shortcut.empty()) {
    operation_type_ = DELEGATE_EXECUTE;
    return true;
  }
  relaunch_shortcut_ = shortcut;
  mutex_ = cmd_line->GetSwitchValueNative(switches::kWaitForMutex);
  if (mutex_.empty())
    return false;
  // Add the mode forcing flags, if any.
  if (cmd_line->HasSwitch(switches::kForceDesktop))
    relaunch_flags_ = switches::kForceDesktop;
  else if (cmd_line->HasSwitch(switches::kForceImmersive))
    relaunch_flags_ = switches::kForceImmersive;

  operation_type_ = RELAUNCH_CHROME;
  return true;
}

DWORD DelegateExecuteOperation::GetParentPid() const {
  std::vector<string16> parts;
  base::SplitString(mutex_, L'.', &parts);
  if (parts.size() != 3)
    return 0;
  DWORD pid;
  if (!base::StringToUint(parts[2], reinterpret_cast<uint32*>(&pid)))
    return 0;
  return pid;
}

}  // namespace delegate_execute
