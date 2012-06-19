// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/message_loop_proxy.h"
#include "net/base/ip_endpoint.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/transport.h"

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

void MockEventExecutor::OnSessionStarted(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  OnSessionStartedPtr(client_clipboard.get());
}

MockDisconnectWindow::MockDisconnectWindow() {}

MockDisconnectWindow::~MockDisconnectWindow() {}

scoped_ptr<DisconnectWindow> DisconnectWindow::Create() {
  return scoped_ptr<DisconnectWindow>(new MockDisconnectWindow());
}

MockContinueWindow::MockContinueWindow() {}

MockContinueWindow::~MockContinueWindow() {}

scoped_ptr<ContinueWindow> ContinueWindow::Create() {
  return scoped_ptr<ContinueWindow>(new MockContinueWindow());
}

MockLocalInputMonitor::MockLocalInputMonitor() {}

MockLocalInputMonitor::~MockLocalInputMonitor() {}

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create() {
  return scoped_ptr<LocalInputMonitor>(new MockLocalInputMonitor());
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
