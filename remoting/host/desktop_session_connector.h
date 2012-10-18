// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
#define REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_

#include "base/basictypes.h"

namespace remoting {

class IpcDesktopEnvironment;

// Provides a way to connect a terminal (i.e. a remote client) with a desktop
// session (i.e. the screen, keyboard and the rest).
class DesktopSessionConnector {
 public:
  DesktopSessionConnector() {}
  virtual ~DesktopSessionConnector() {}

  // Requests the daemon process to create a desktop session and associates
  // |desktop_environment| with it.
  virtual void ConnectTerminal(IpcDesktopEnvironment* desktop_environment) = 0;

  // Requests the daemon process disconnect |desktop_environment| from
  // the associated desktop session.
  virtual void DisconnectTerminal(
      IpcDesktopEnvironment* desktop_environment) = 0;

  // Notifies the network process that the daemon has disconnected the desktop
  // session from the associated descktop environment.
  virtual void OnTerminalDisconnected(int terminal_id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopSessionConnector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_CONNECTOR_H_
