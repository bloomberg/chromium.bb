// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include "net/base/ip_endpoint.h"
#include "remoting/base/capture_data.h"
#include "remoting/host/video_frame_capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/desktop_environment_factory.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/local_input_monitor.h"
#include "remoting/host/ui_strings.h"
#include "remoting/proto/control.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockVideoFrameCapturer : public VideoFrameCapturer {
 public:
  MockVideoFrameCapturer();
  virtual ~MockVideoFrameCapturer();

  MOCK_METHOD1(Start, void(Delegate* delegate));
  MOCK_METHOD0(Stop, void());
  MOCK_CONST_METHOD0(pixel_format, media::VideoFrame::Format());
  MOCK_METHOD1(InvalidateRegion, void(const SkRegion& invalid_region));
  MOCK_METHOD0(CaptureFrame, void());
  MOCK_CONST_METHOD0(size_most_recent, const SkISize&());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameCapturer);
};

class MockVideoFrameCapturerDelegate : public VideoFrameCapturer::Delegate {
 public:
  MockVideoFrameCapturerDelegate();
  virtual ~MockVideoFrameCapturerDelegate();

  virtual void OnCursorShapeChanged(
      scoped_ptr<protocol::CursorShapeInfo> cursor_shape) OVERRIDE;

  MOCK_METHOD1(OnCaptureCompleted, void(scoped_refptr<CaptureData>));
  MOCK_METHOD1(OnCursorShapeChangedPtr, void(protocol::CursorShapeInfo*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameCapturerDelegate);
};

class MockDisconnectWindow : public DisconnectWindow {
 public:
  MockDisconnectWindow();
  virtual ~MockDisconnectWindow();

  MOCK_METHOD3(Show, bool(const UiStrings& ui_strings,
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
  MOCK_METHOD2(OnClientDimensionsChanged, void(ClientSession* client,
                                               const SkISize& size));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSessionEventHandler);
};

class MockDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  MockDesktopEnvironmentFactory();
  virtual ~MockDesktopEnvironmentFactory();

  MOCK_METHOD1(CreatePtr, DesktopEnvironment*(ClientSession* client));

  virtual scoped_ptr<DesktopEnvironment> Create(ClientSession* client) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDesktopEnvironmentFactory);
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

  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);

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

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_MOCK_OBJECTS_H_
