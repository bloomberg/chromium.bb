// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_VIEW_H_
#define REMOTING_CLIENT_CHROMOTING_VIEW_H_

#include "base/basictypes.h"
#include "remoting/protocol/connection_to_host.h"

namespace remoting {

namespace protocol {
class ClipboardStub;
class CursorShapeStub;
}  // namespace protocol

// ChromotingView defines the behavior of an object that draws a view of the
// remote desktop. Its main function is to render the update stream onto the
// screen.
class ChromotingView {
 public:
  virtual ~ChromotingView() {}

  // Initialize the common structures for the view.
  virtual bool Initialize() = 0;

  // Free up resources allocated by this view.
  virtual void TearDown() = 0;

  // Record the update the state of the connection, updating the UI as needed.
  virtual void SetConnectionState(protocol::ConnectionToHost::State state,
                                  protocol::ErrorCode error) = 0;

  // Get the view's ClipboardStub implementation.
  virtual protocol::ClipboardStub* GetClipboardStub() = 0;

  // Get the view's CursorShapeStub implementation.
  virtual protocol::CursorShapeStub* GetCursorShapeStub() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_VIEW_H_
