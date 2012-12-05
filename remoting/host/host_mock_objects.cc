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

MockVideoFrameCapturerDelegate::MockVideoFrameCapturerDelegate() {
}

MockVideoFrameCapturerDelegate::~MockVideoFrameCapturerDelegate() {
}

void MockVideoFrameCapturerDelegate::OnCursorShapeChanged(
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  // Notify the mock method.
  OnCursorShapeChangedPtr(cursor_shape.get());
}

MockDesktopEnvironmentFactory::MockDesktopEnvironmentFactory()
    : DesktopEnvironmentFactory(NULL, NULL) {
}

MockDesktopEnvironmentFactory::~MockDesktopEnvironmentFactory() {}

scoped_ptr<DesktopEnvironment> MockDesktopEnvironmentFactory::Create(
    ClientSession* client) {
  return scoped_ptr<DesktopEnvironment>(CreatePtr(client));
}

MockEventExecutor::MockEventExecutor() {}

MockEventExecutor::~MockEventExecutor() {}

void MockEventExecutor::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  StartPtr(client_clipboard.get());
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

MockClientSessionEventHandler::MockClientSessionEventHandler() {}

MockClientSessionEventHandler::~MockClientSessionEventHandler() {}

MockHostStatusObserver::MockHostStatusObserver() {}

MockHostStatusObserver::~MockHostStatusObserver() {}

}  // namespace remoting
