// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_H_
#define REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionControl;

// Monitors the local input to notify about keyboard hotkeys. If implemented for
// the platform, catches the disconnection keyboard shortcut (Ctlr-Alt-Esc) and
// invokes SessionController::Delegate::DisconnectSession() when this key
// combination is pressed.
class LocalHotkeyInputMonitor {
 public:
  virtual ~LocalHotkeyInputMonitor() = default;

  // Creates a platform-specific instance of LocalHotkeyInputMonitor.
  // |client_session_control| is called on the |caller_task_runner| thread.
  static std::unique_ptr<LocalHotkeyInputMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);

 protected:
  LocalHotkeyInputMonitor() = default;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_LOCAL_HOTKEY_INPUT_MONITOR_H_
