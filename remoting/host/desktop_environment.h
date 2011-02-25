// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "remoting/protocol/host_stub.h"

namespace remoting {

namespace protocol {
class InputStub;
}  // namespace protocol

class Capturer;

class DesktopEnvironment : public protocol::HostStub {
 public:
  // Callback interface for passing events to the ChromotingHost.
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called to signal that local login has succeeded and ChromotingHost can
    // proceed with the next step.
    virtual void LocalLoginSucceeded() = 0;
  };

  DesktopEnvironment(Capturer* capturer, protocol::InputStub* input_stub);
  virtual ~DesktopEnvironment();

  Capturer* capturer() const { return capturer_.get(); }
  protocol::InputStub* input_stub() const { return input_stub_.get(); }
  // Called by ChromotingHost constructor
  void set_event_handler(EventHandler* event_handler) {
    event_handler_ = event_handler;
  }

  // protocol::HostStub interface.
  virtual void SuggestResolution(
      const protocol::SuggestResolutionRequest* msg, Task* done);
  virtual void BeginSessionRequest(
      const protocol::LocalLoginCredentials* credentials, Task* done);

 protected:
  // Allow access by DesktopEnvironmentFake for unittest.
  EventHandler* event_handler_;

 private:
  // Capturer to be used by ScreenRecorder. Once the ScreenRecorder is
  // constructed this is set to NULL.
  scoped_ptr<Capturer> capturer_;

  // InputStub in the host executes input events received from the client.
  scoped_ptr<protocol::InputStub> input_stub_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
