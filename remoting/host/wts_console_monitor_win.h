// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WTS_CONSOLE_MONITOR_WIN_H_
#define REMOTING_HOST_WTS_CONSOLE_MONITOR_WIN_H_

#include <windows.h>

#include "base/basictypes.h"

namespace remoting {

class WtsConsoleObserver;

class WtsConsoleMonitor {
 public:
  virtual ~WtsConsoleMonitor() {}

  // Registers an observer to receive notifications about a particular WTS
  // console (i.e. the physical console or a remote console).
  virtual void AddWtsConsoleObserver(WtsConsoleObserver* observer) = 0;

  // Unregisters a previously registered observer.
  virtual void RemoveWtsConsoleObserver(WtsConsoleObserver* observer) = 0;

 protected:
  WtsConsoleMonitor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WtsConsoleMonitor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WTS_CONSOLE_MONITOR_WIN_H_
