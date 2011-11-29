// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/message_loop_proxy.h"
#include "remoting/proto/event.pb.h"

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

MockChromotingHostContext::MockChromotingHostContext()
    : ChromotingHostContext(base::MessageLoopProxy::current()) {
}

MockChromotingHostContext::~MockChromotingHostContext() {}

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockUserAuthenticator::MockUserAuthenticator() {}

MockUserAuthenticator::~MockUserAuthenticator() {}

}  // namespace remoting
