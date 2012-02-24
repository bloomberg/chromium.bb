// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the Windows service controlling Me2Me host processes
// running within user sessions.

#include "remoting/host/wts_session_process_launcher_win.h"

#include <windows.h>

#include "base/logging.h"

#include "remoting/host/wts_console_monitor_win.h"

namespace remoting {

WtsSessionProcessLauncher::WtsSessionProcessLauncher(
    WtsConsoleMonitor* monitor) : monitor_(monitor) {
  monitor_->AddWtsConsoleObserver(this);
}

WtsSessionProcessLauncher::~WtsSessionProcessLauncher() {
  monitor_->RemoveWtsConsoleObserver(this);
}

void WtsSessionProcessLauncher::OnSessionAttached(uint32 session_id) {
  // TODO(alexeypa): The code injecting the host process to a session goes here.
}

void WtsSessionProcessLauncher::OnSessionDetached() {
  // TODO(alexeypa): The code terminating the host process goes here.
}

} // namespace remoting
