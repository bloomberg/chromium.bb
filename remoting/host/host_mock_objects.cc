// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_mock_objects.h"

#include "base/message_loop_proxy.h"
#include "net/base/ip_endpoint.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/base/capture_data.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/transport.h"

namespace remoting {

MockVideoFrameCapturer::MockVideoFrameCapturer() {}

MockVideoFrameCapturer::~MockVideoFrameCapturer() {}

MockCaptureCompletedCallback::MockCaptureCompletedCallback() {}

MockCaptureCompletedCallback::~MockCaptureCompletedCallback() {}

void MockCaptureCompletedCallback::CaptureCompleted(
    scoped_refptr<CaptureData> capture_data) {
  CaptureCompletedPtr(capture_data.get());
}

MockEventExecutor::MockEventExecutor() {}

MockEventExecutor::~MockEventExecutor() {}

void MockEventExecutor::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  StartPtr(client_clipboard.get());
}

void MockEventExecutor::StopAndDelete() {
  StopAndDeleteMock();
  delete this;
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
    : ChromotingHostContext(new AutoThreadTaskRunner(
          base::MessageLoopProxy::current())) {
}

MockChromotingHostContext::~MockChromotingHostContext() {}

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockHostStatusObserver::MockHostStatusObserver() {}

MockHostStatusObserver::~MockHostStatusObserver() {}

MockUserAuthenticator::MockUserAuthenticator() {}

MockUserAuthenticator::~MockUserAuthenticator() {}

}  // namespace remoting
