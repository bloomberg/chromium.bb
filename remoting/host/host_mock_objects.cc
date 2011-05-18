// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

namespace remoting {

MockCapturer::MockCapturer() {}

MockCapturer::~MockCapturer() {}

MockCurtain::MockCurtain() {}

MockCurtain::~MockCurtain() {}

MockEventExecutor::MockEventExecutor() {}

MockEventExecutor::~MockEventExecutor() {}

Curtain* Curtain::Create() {
  return new MockCurtain();
}

MockChromotingHostContext::MockChromotingHostContext() {}

MockChromotingHostContext::~MockChromotingHostContext() {}

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockUserAuthenticator::MockUserAuthenticator() {}

MockUserAuthenticator::~MockUserAuthenticator() {}

MockAccessVerifier::MockAccessVerifier() {}

MockAccessVerifier::~MockAccessVerifier() {}

}  // namespace remoting
