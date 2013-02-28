// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_
#define REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_

#include <windows.h>

#include "base/basictypes.h"
#include "net/base/ip_endpoint.h"

namespace remoting {

class WtsTerminalObserver;

class WtsTerminalMonitor {
 public:
  virtual ~WtsTerminalMonitor() {}

  // Registers an observer to receive notifications about a particular WTS
  // terminal. To speficy the physical console the caller should pass
  // net::IPEndPoint() to |client_endpoint|. Otherwise an RDP connection from
  // the given endpoint will be monitored. Each observer instance can
  // monitor a single WTS console.
  // Returns |true| of success. Returns |false| if |observer| is already
  // registered.
  virtual bool AddWtsTerminalObserver(const net::IPEndPoint& client_endpoint,
                                      WtsTerminalObserver* observer) = 0;

  // Unregisters a previously registered observer.
  virtual void RemoveWtsTerminalObserver(WtsTerminalObserver* observer) = 0;

 protected:
  WtsTerminalMonitor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WtsTerminalMonitor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_
