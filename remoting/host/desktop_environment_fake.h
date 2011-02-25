// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_FAKE_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_FAKE_H_

#include "remoting/host/desktop_environment.h"

namespace remoting {

// A DesktopEnvironmentFake allows connection to proceed for unit-testing,
// without needing a valid user/password for the system.
class DesktopEnvironmentFake : public DesktopEnvironment {
 public:
  DesktopEnvironmentFake(Capturer* capturer, protocol::InputStub* input_stub);
  virtual ~DesktopEnvironmentFake();

  // Overridden to do no authentication.
  virtual void BeginSessionRequest(
      const protocol::LocalLoginCredentials* credentials, Task* done);

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironmentFake);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_FAKE_H_
