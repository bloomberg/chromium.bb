// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include "net/base/ip_endpoint.h"
#include "remoting/host/video_frame_capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/user_authenticator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockCaptureCompletedCallback {
 public:
  MockCaptureCompletedCallback();
  virtual ~MockCaptureCompletedCallback();

  MOCK_METHOD1(CaptureCompletedPtr, void(CaptureData* capture_data));
  void CaptureCompleted(scoped_refptr<CaptureData> capture_data);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCaptureCompletedCallback);
};

class MockVideoFrameCapturer : public VideoFrameCapturer {
 public:
  MockVideoFrameCapturer();
  virtual ~MockVideoFrameCapturer();

  MOCK_METHOD1(Start, void(const CursorShapeChangedCallback& callback));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(pixel_format, media::VideoFrame::Format());
  MOCK_METHOD1(InvalidateRegion, void(const SkRegion& invalid_region));
  MOCK_METHOD1(CaptureInvalidRegion,
               void(const CaptureCompletedCallback& callback));
  MOCK_CONST_METHOD0(size_most_recent, const SkISize&());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameCapturer);
};

class MockDisconnectWindow : public DisconnectWindow {
 public:
  MockDisconnectWindow();
  virtual ~MockDisconnectWindow();

  MOCK_METHOD3(Show, void(remoting::ChromotingHost* host,
                          const DisconnectCallback& disconnect_callback,
                          const std::string& username));
  MOCK_METHOD0(Hide, void());
};

class MockLocalInputMonitor : public LocalInputMonitor {
 public:
  MockLocalInputMonitor();
  virtual ~MockLocalInputMonitor();

  MOCK_METHOD2(Start, void(MouseMoveObserver* mouse_move_observer,
                           const base::Closure& disconnect_callback));
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

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(ui_task_runner, base::SingleThreadTaskRunner*());
  MOCK_METHOD0(capture_task_runner, base::SingleThreadTaskRunner*());
  MOCK_METHOD0(encode_task_runner, base::SingleThreadTaskRunner*());
  MOCK_METHOD0(network_task_runner, base::SingleThreadTaskRunner*());
  MOCK_METHOD0(io_task_runner, base::SingleThreadTaskRunner*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChromotingHostContext);
};

class MockClientSessionEventHandler : public ClientSession::EventHandler {
 public:
  MockClientSessionEventHandler();
  virtual ~MockClientSessionEventHandler();

  MOCK_METHOD1(OnSessionAuthenticated, void(ClientSession* client));
  MOCK_METHOD1(OnSessionChannelsConnected, void(ClientSession* client));
  MOCK_METHOD1(OnSessionAuthenticationFailed, void(ClientSession* client));
  MOCK_METHOD1(OnSessionClosed, void(ClientSession* client));
  MOCK_METHOD2(OnSessionSequenceNumber, void(ClientSession* client,
                                             int64 sequence_number));
  MOCK_METHOD3(OnSessionRouteChange, void(
      ClientSession* client,
      const std::string& channel_name,
      const protocol::TransportRoute& route));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSessionEventHandler);
};

class MockEventExecutor : public EventExecutor {
 public:
  MockEventExecutor();
  virtual ~MockEventExecutor();

  MOCK_METHOD1(InjectClipboardEvent,
               void(const protocol::ClipboardEvent& event));
  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent& event));
  MOCK_METHOD1(StartPtr,
               void(protocol::ClipboardStub* client_clipboard));
  MOCK_METHOD0(StopAndDeleteMock, void());

  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);
  void StopAndDelete();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventExecutor);
};

class MockHostStatusObserver : public HostStatusObserver {
 public:
  MockHostStatusObserver();
  virtual ~MockHostStatusObserver();

  MOCK_METHOD1(OnAccessDenied, void(const std::string& jid));
  MOCK_METHOD1(OnClientAuthenticated, void(const std::string& jid));
  MOCK_METHOD1(OnClientConnected, void(const std::string& jid));
  MOCK_METHOD1(OnClientDisconnected, void(const std::string& jid));
  MOCK_METHOD3(OnClientRouteChange,
               void(const std::string& jid,
                    const std::string& channel_name,
                    const protocol::TransportRoute& route));
  MOCK_METHOD1(OnStart, void(const std::string& xmpp_login));
  MOCK_METHOD0(OnShutdown, void());
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
