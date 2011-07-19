// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

namespace remoting {

MockCapturer::MockCapturer() {}

MockCapturer::~MockCapturer() {}

MockCurtain::MockCurtain() {}

MockCurtain::~MockCurtain() {}

Curtain* Curtain::Create() {
  return new MockCurtain();
}

MockEventExecutor::MockEventExecutor() {}

MockEventExecutor::~MockEventExecutor() {}

MockDisconnectWindow::MockDisconnectWindow() {}

MockDisconnectWindow::~MockDisconnectWindow() {}

DisconnectWindow* DisconnectWindow::Create() {
  return new MockDisconnectWindow();
}

MockContinueWindow::MockContinueWindow() {}

MockContinueWindow::~MockContinueWindow() {}

ContinueWindow* ContinueWindow::Create() {
  return new MockContinueWindow();
}

MockLocalInputMonitor::MockLocalInputMonitor() {}

MockLocalInputMonitor::~MockLocalInputMonitor() {}

LocalInputMonitor* LocalInputMonitor::Create() {
  return new MockLocalInputMonitor();
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
