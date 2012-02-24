// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_wts_console_observer_win_H_
#define REMOTING_HOST_wts_console_observer_win_H_

#include <windows.h>

#include "base/basictypes.h"

namespace remoting {

// Provides callbacks for monitoring events on a WTS terminal.
class WtsConsoleObserver {
 public:
  virtual ~WtsConsoleObserver() {}

  // Called when |session_id| attaches to the console.
  virtual void OnSessionAttached(uint32 session_id) = 0;

  // Called when a session detaches from the console.
  virtual void OnSessionDetached() = 0;

 protected:
  WtsConsoleObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WtsConsoleObserver);
};

}  // namespace remoting

#endif  // REMOTING_HOST_wts_console_observer_win_H_
