// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/delegate_execute_operation.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace delegate_execute {

DelegateExecuteOperation::DelegateExecuteOperation()
    : operation_type_(EXE_MODULE) {
}

DelegateExecuteOperation::~DelegateExecuteOperation() {
  Clear();
}

void DelegateExecuteOperation::Initialize(const CommandLine* command_line) {
  Clear();

  // --relaunch-shortcut=PathToShortcut triggers the relaunch Chrome operation.
  FilePath shortcut(
      command_line->GetSwitchValuePath(switches::kRelaunchShortcut));
  if (!shortcut.empty()) {
    relaunch_shortcut_ = shortcut;
    operation_type_ = RELAUNCH_CHROME;
  }
}

void DelegateExecuteOperation::Clear() {
  operation_type_ = EXE_MODULE;
  relaunch_shortcut_.clear();
}

}  // namespace delegate_execute
