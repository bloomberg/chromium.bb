// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_
#define REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_

#include "base/basictypes.h"

namespace net {
class IPEndPoint;
}  // namespace net

namespace remoting {

class WtsTerminalObserver;

// Session id that does not represent any session.
extern const uint32 kInvalidSessionId;

class WtsTerminalMonitor {
 public:
  virtual ~WtsTerminalMonitor();

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

  // Sets |*endpoint| to the endpoint of the client attached to |session_id|.
  // If |session_id| is attached to the physical console net::IPEndPoint() is
  // used. Returns false if the endpoint cannot be queried (if there is no
  // client attached to |session_id| for instance).
  static bool GetEndpointForSessionId(uint32 session_id,
                                      net::IPEndPoint* endpoint);

  // Returns id of the session that |client_endpoint| is attached.
  // |kInvalidSessionId| is returned if none of the sessions is currently
  // attahced to |client_endpoint|.
  static uint32 GetSessionIdForEndpoint(const net::IPEndPoint& client_endpoint);

 protected:
  WtsTerminalMonitor();

 private:
  DISALLOW_COPY_AND_ASSIGN(WtsTerminalMonitor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WTS_TERMINAL_MONITOR_H_
