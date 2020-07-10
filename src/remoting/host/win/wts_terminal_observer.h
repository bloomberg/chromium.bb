// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_TERMINAL_OBSERVER_H_
#define REMOTING_HOST_WIN_WTS_TERMINAL_OBSERVER_H_

#include <windows.h>
#include <stdint.h>

#include "base/macros.h"

namespace remoting {

// Provides callbacks for monitoring events on a WTS terminal.
class WtsTerminalObserver {
 public:
  virtual ~WtsTerminalObserver() {}

  // Called when |session_id| attaches to the console.
  virtual void OnSessionAttached(uint32_t session_id) = 0;

  // Called when a session detaches from the console.
  virtual void OnSessionDetached() = 0;

 protected:
  WtsTerminalObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WtsTerminalObserver);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_TERMINAL_OBSERVER_H_
