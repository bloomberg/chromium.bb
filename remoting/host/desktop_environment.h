// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace remoting {

namespace protocol {
class InputStub;
}  // namespace protocol

class Capturer;

class DesktopEnvironment {
 public:
  DesktopEnvironment(Capturer* capturer, protocol::InputStub* input_stub);
  virtual ~DesktopEnvironment();

  Capturer* capturer() const { return capturer_.get(); }
  protocol::InputStub* input_stub() const { return input_stub_.get(); }

 private:
  // Capturer to be used by ScreenRecorder.
  scoped_ptr<Capturer> capturer_;

  // InputStub in the host executes input events received from the client.
  scoped_ptr<protocol::InputStub> input_stub_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
