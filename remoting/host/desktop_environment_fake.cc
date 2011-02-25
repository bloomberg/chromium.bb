// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment_fake.h"

#include "base/task.h"

using remoting::protocol::InputStub;

namespace remoting {

DesktopEnvironmentFake::DesktopEnvironmentFake(Capturer* capturer,
                                               InputStub* input_stub)
    : DesktopEnvironment(capturer, input_stub) {
}

DesktopEnvironmentFake::~DesktopEnvironmentFake() {}

void DesktopEnvironmentFake::BeginSessionRequest(
    const protocol::LocalLoginCredentials* credentials, Task* done) {
  DCHECK(event_handler_);

  // For unit-test, don't require a valid Unix user/password for connection.
  event_handler_->LocalLoginSucceeded();

  done->Run();
  delete done;
}

}  // namespace remoting
