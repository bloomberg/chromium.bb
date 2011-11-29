// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include "remoting/host/capturer.h"
#include "remoting/host/curtain.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/user_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockCapturer : public Capturer {
 public:
  MockCapturer();
  virtual ~MockCapturer();

  MOCK_METHOD0(ScreenConfigurationChanged, void());
  MOCK_CONST_METHOD0(pixel_format, media::VideoFrame::Format());
  MOCK_METHOD0(ClearInvalidRegion, void());
  MOCK_METHOD1(InvalidateRegion, void(const SkRegion& invalid_region));
  MOCK_METHOD1(InvalidateScreen, void(const SkISize&));
  MOCK_METHOD0(InvalidateFullScreen, void());
  MOCK_METHOD1(CaptureInvalidRegion,
               void(const CaptureCompletedCallback& callback));
  MOCK_CONST_METHOD0(size_most_recent, const SkISize&());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCapturer);
};

class MockCurtain : public Curtain {
 public:
  MockCurtain();
  virtual ~MockCurtain();

  MOCK_METHOD1(EnableCurtainMode, void(bool enable));
};

class MockDisconnectWindow : public DisconnectWindow {
 public:
  MockDisconnectWindow();
  virtual ~MockDisconnectWindow();

  MOCK_METHOD2(Show, void(remoting::ChromotingHost* host,
                          const std::string& username));
  MOCK_METHOD0(Hide, void());
};

class MockLocalInputMonitor : public LocalInputMonitor {
 public:
  MockLocalInputMonitor();
  virtual ~MockLocalInputMonitor();

  MOCK_METHOD1(Start, void(remoting::ChromotingHost* host));
  MOCK_METHOD0(Stop, void());
};

class MockContinueWindow : public ContinueWindow {
 public:
  MockContinueWindow();
  virtual ~MockContinueWindow();

  MOCK_METHOD2(Show, void(
      remoting::ChromotingHost* host,
      const remoting::ContinueWindow::ContinueSessionCallback& callback));
  MOCK_METHOD0(Hide, void());
};

class MockChromotingHostContext : public ChromotingHostContext {
 public:
  MockChromotingHostContext();
  virtual ~MockChromotingHostContext();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(jingle_thread, JingleThread*());
  MOCK_METHOD0(ui_message_loop, base::MessageLoopProxy*());
  MOCK_METHOD0(main_message_loop, MessageLoop*());
  MOCK_METHOD0(encode_message_loop, MessageLoop*());
  MOCK_METHOD0(network_message_loop, base::MessageLoopProxy*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChromotingHostContext);
};

class MockClientSessionEventHandler : public ClientSession::EventHandler {
 public:
  MockClientSessionEventHandler();
  virtual ~MockClientSessionEventHandler();

  MOCK_METHOD1(OnSessionAuthenticated, void(ClientSession* client));
  MOCK_METHOD1(OnSessionAuthenticationFailed, void(ClientSession* client));
  MOCK_METHOD1(OnSessionClosed, void(ClientSession* client));
  MOCK_METHOD1(OnSessionFailed, void(ClientSession* client));
  MOCK_METHOD2(OnSessionSequenceNumber, void(ClientSession* client,
                                             int64 sequence_number));
 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSessionEventHandler);
};

class MockEventExecutor : public EventExecutor {
 public:
  MockEventExecutor();
  virtual ~MockEventExecutor();

  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventExecutor);
};

class MockUserAuthenticator : public UserAuthenticator {
 public:
  MockUserAuthenticator();
  virtual ~MockUserAuthenticator();

  MOCK_METHOD2(Authenticate, bool(const std::string& username,
                                  const std::string& password));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUserAuthenticator);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_MOCK_OBJECTS_H_
