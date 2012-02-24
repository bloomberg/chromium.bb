// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_
#define REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

#include "remoting/host/wts_console_observer_win.h"

namespace remoting {

class WtsConsoleMonitor;

class WtsSessionProcessLauncher : public WtsConsoleObserver {
 public:
  // Constructs a WtsSessionProcessLauncher object. |monitor| must outlive this
  // class.
  WtsSessionProcessLauncher(WtsConsoleMonitor* monitor);
  virtual ~WtsSessionProcessLauncher();

  // WtsConsoleObserver implementation
  virtual void OnSessionAttached(uint32 session_id) OVERRIDE;
  virtual void OnSessionDetached() OVERRIDE;

 private:
  WtsConsoleMonitor* monitor_;

  DISALLOW_COPY_AND_ASSIGN(WtsSessionProcessLauncher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WTS_SESSION_PROCESS_LAUNCHER_WIN_H_
