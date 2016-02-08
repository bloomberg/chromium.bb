// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include <string>

#include "base/macros.h"
#include "net/base/ip_endpoint.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/host/security_key/gnubby_auth_handler.h"
#include "remoting/proto/control.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class MockDesktopEnvironment : public DesktopEnvironment {
 public:
  MockDesktopEnvironment();
  ~MockDesktopEnvironment() override;

  MOCK_METHOD0(CreateAudioCapturerPtr, AudioCapturer*());
  MOCK_METHOD0(CreateInputInjectorPtr, InputInjector*());
  MOCK_METHOD0(CreateScreenControlsPtr, ScreenControls*());
  MOCK_METHOD0(CreateVideoCapturerPtr, webrtc::DesktopCapturer*());
  MOCK_METHOD0(CreateMouseCursorMonitorPtr, webrtc::MouseCursorMonitor*());
  MOCK_CONST_METHOD0(GetCapabilities, std::string());
  MOCK_METHOD1(SetCapabilities, void(const std::string&));
  MOCK_METHOD1(CreateGnubbyAuthHandlerPtr, GnubbyAuthHandler*(
      protocol::ClientStub* client_stub));

  // DesktopEnvironment implementation.
  scoped_ptr<AudioCapturer> CreateAudioCapturer() override;
  scoped_ptr<InputInjector> CreateInputInjector() override;
  scoped_ptr<ScreenControls> CreateScreenControls() override;
  scoped_ptr<webrtc::DesktopCapturer> CreateVideoCapturer() override;
  scoped_ptr<GnubbyAuthHandler> CreateGnubbyAuthHandler(
      protocol::ClientStub* client_stub) override;
  scoped_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
};

class MockClientSessionControl : public ClientSessionControl {
 public:
  MockClientSessionControl();
  ~MockClientSessionControl() override;

  MOCK_CONST_METHOD0(client_jid, const std::string&());
  MOCK_METHOD1(DisconnectSession, void(protocol::ErrorCode error));
  MOCK_METHOD1(OnLocalMouseMoved, void(const webrtc::DesktopVector&));
  MOCK_METHOD1(SetDisableInputs, void(bool));
  MOCK_METHOD0(ResetVideoPipeline, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSessionControl);
};

class MockClientSessionEventHandler : public ClientSession::EventHandler {
 public:
  MockClientSessionEventHandler();
  ~MockClientSessionEventHandler() override;

  MOCK_METHOD1(OnSessionAuthenticating, void(ClientSession* client));
  MOCK_METHOD1(OnSessionAuthenticated, void(ClientSession* client));
  MOCK_METHOD1(OnSessionChannelsConnected, void(ClientSession* client));
  MOCK_METHOD1(OnSessionAuthenticationFailed, void(ClientSession* client));
  MOCK_METHOD1(OnSessionClosed, void(ClientSession* client));
  MOCK_METHOD3(OnSessionRouteChange, void(
      ClientSession* client,
      const std::string& channel_name,
      const protocol::TransportRoute& route));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSessionEventHandler);
};

class MockDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  MockDesktopEnvironmentFactory();
  ~MockDesktopEnvironmentFactory() override;

  MOCK_METHOD0(CreatePtr, DesktopEnvironment*());
  MOCK_CONST_METHOD0(SupportsAudioCapture, bool());

  scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDesktopEnvironmentFactory);
};

class MockInputInjector : public InputInjector {
 public:
  MockInputInjector();
  ~MockInputInjector() override;

  MOCK_METHOD1(InjectClipboardEvent,
               void(const protocol::ClipboardEvent& event));
  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent& event));
  MOCK_METHOD1(InjectTextEvent, void(const protocol::TextEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent& event));
  MOCK_METHOD1(InjectTouchEvent, void(const protocol::TouchEvent& event));
  MOCK_METHOD1(StartPtr,
               void(protocol::ClipboardStub* client_clipboard));

  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputInjector);
};

class MockHostStatusObserver : public HostStatusObserver {
 public:
  MockHostStatusObserver();
  ~MockHostStatusObserver() override;

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

class MockGnubbyAuthHandler : public GnubbyAuthHandler {
 public:
  MockGnubbyAuthHandler();
  ~MockGnubbyAuthHandler() override;

  MOCK_METHOD1(DeliverClientMessage, void(const std::string& message));
  MOCK_CONST_METHOD2(DeliverHostDataMessage,
                     void(int connection_id, const std::string& data));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGnubbyAuthHandler);
};

class MockMouseCursorMonitor : public webrtc::MouseCursorMonitor {
 public:
  MockMouseCursorMonitor();
  ~MockMouseCursorMonitor() override;

  MOCK_METHOD2(Init, void(Callback* callback, Mode mode));
  MOCK_METHOD0(Capture, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMouseCursorMonitor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_MOCK_OBJECTS_H_
