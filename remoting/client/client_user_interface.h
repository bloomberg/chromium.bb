// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_
#define REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_

#include "base/basictypes.h"
#include "remoting/protocol/connection_to_host.h"

namespace remoting {

namespace protocol {
class ClipboardStub;
class CursorShapeStub;
}  // namespace protocol

// ClientUserInterface is an interface that must be implemented by
// applications embedding the Chromoting client, to provide client's user
// interface.
//
// TODO(sergeyu): Cleanup this interface, see crbug.com/138108 .
class ClientUserInterface {
 public:
  virtual ~ClientUserInterface() {}

  // Record the update the state of the connection, updating the UI as needed.
  virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                 protocol::ErrorCode error) = 0;
  virtual void OnConnectionReady(bool ready) = 0;

  // Get the view's ClipboardStub implementation.
  virtual protocol::ClipboardStub* GetClipboardStub() = 0;

  // Get the view's CursorShapeStub implementation.
  virtual protocol::CursorShapeStub* GetCursorShapeStub() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_USER_INTERFACE_H_
