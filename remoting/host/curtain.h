// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CURTAIN_H_
#define REMOTING_HOST_CURTAIN_H_

namespace remoting {

// An interface for enabling or disabling "curtain mode" on a Chromoting host.
// Curtain mode is designed to ensure privacy for remote users. It guarantees
// the following:
//   1. The local display of the host does not display the remote user's
//      actions during the connection.
//   2. The local keyboard and mouse (and other input devices) do not interfere
//      with the remote user's session.
//   3. When the remote session terminates, the host computer is left in a
//      secure state (for example, locked).
class Curtain {
 public:
  virtual ~Curtain() { }

  // Enable or disable curtain mode. This method is called with |enable| = true
  // when a connection authenticates and with |enable| = false when a connection
  // terminates (even if due to abnormal termination of the host process).
  virtual void EnableCurtainMode(bool enable) = 0;

  // Create the platform-specific curtain mode implementation.
  // TODO(jamiewalch): Until the daemon architecture is implemented, curtain
  // mode implementations that cannot easily be reset by the user should check
  // to see if curtain mode is already enabled here and disable it if so. This
  // is to provide an easy way of recovering if the host process crashes while
  // a connection is active. Once the daemon architecture is in place, it will
  // be responsible for calling EnableCurtainMode(false) as part of its crash
  // recovery logic.
  static Curtain* Create();
};

}  // namespace remoting

#endif  // REMOTING_HOST_CURTAIN_H_
