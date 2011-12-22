// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_VIEW_H_
#define REMOTING_CLIENT_CHROMOTING_VIEW_H_

#include "base/basictypes.h"
#include "remoting/protocol/connection_to_host.h"

namespace remoting {

static const uint32 kCreatedColor = 0xffccccff;
static const uint32 kDisconnectedColor = 0xff00ccff;
static const uint32 kFailedColor = 0xffcc00ff;

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

  // Tells the ChromotingView to paint the current image on the screen.
  virtual void Paint() = 0;

  // Fill the screen with one single static color, and ignore updates.
  // Useful for debugging.
  virtual void SetSolidFill(uint32 color) = 0;

  // Removes a previously set solid fill.  If no fill was previous set, this
  // does nothing.
  virtual void UnsetSolidFill() = 0;

  // Record the update the state of the connection, updating the UI as needed.
  virtual void SetConnectionState(protocol::ConnectionToHost::State state,
                                  protocol::ConnectionToHost::Error error) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_VIEW_H_
