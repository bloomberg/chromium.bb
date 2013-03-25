// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include "net/base/ip_endpoint.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/screen_controls.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/control.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class MockDesktopEnvironment : public DesktopEnvironment {
 public:
  MockDesktopEnvironment();
  virtual ~MockDesktopEnvironment();

  MOCK_METHOD0(CreateAudioCapturerPtr, AudioCapturer*());
  MOCK_METHOD0(CreateInputInjectorPtr, InputInjector*());
  MOCK_METHOD0(CreateScreenControlsPtr, ScreenControls*());
  MOCK_METHOD0(CreateVideoCapturerPtr, media::ScreenCapturer*());

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer() OVERRIDE;
  virtual scoped_ptr<InputInjector> CreateInputInjector() OVERRIDE;
  virtual scoped_ptr<ScreenControls> CreateScreenControls() OVERRIDE;
  virtual scoped_ptr<media::ScreenCapturer> CreateVideoCapturer() OVERRIDE;
};

class MockDisconnectWindow : public DisconnectWindow {
 public:
  MockDisconnectWindow();
  virtual ~MockDisconnectWindow();

  MOCK_METHOD2(Show, bool(const base::Closure& disconnect_callback,
                          const std::string& username));
  MOCK_METHOD0(Hide, void());
};

class MockContinueWindow : public ContinueWindow {
 public:
  MockContinueWindow();
  virtual ~MockContinueWindow();

  MOCK_METHOD1(Show, void(
      const remoting::ContinueWindow::ContinueSessionCallback& callback));
  MOCK_METHOD0(Hide, void());
};

class MockClientSessionControl : public ClientSessionControl {
 public:
  MockClientSessionControl();
  virtual ~MockClientSessionControl();

  MOCK_CONST_METHOD0(client_jid, const std::string&());
  MOCK_METHOD0(DisconnectSession, void());
  MOCK_METHOD1(OnLocalMouseMoved, void(const SkIPoint&));
  MOCK_METHOD1(SetDisableInputs, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientSessionControl);
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

class MockDesktopEnvironmentFactory : public DesktopEnvironmentFactory {
 public:
  MockDesktopEnvironmentFactory();
  virtual ~MockDesktopEnvironmentFactory();

  MOCK_METHOD0(CreatePtr, DesktopEnvironment*());
  MOCK_CONST_METHOD0(SupportsAudioCapture, bool());

  virtual scoped_ptr<DesktopEnvironment> Create(
      base::WeakPtr<ClientSessionControl> client_session_control) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDesktopEnvironmentFactory);
};

class MockInputInjector : public InputInjector {
 public:
  MockInputInjector();
  virtual ~MockInputInjector();

  MOCK_METHOD1(InjectClipboardEvent,
               void(const protocol::ClipboardEvent& event));
  MOCK_METHOD1(InjectKeyEvent, void(const protocol::KeyEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const protocol::MouseEvent& event));
  MOCK_METHOD1(StartPtr,
               void(protocol::ClipboardStub* client_clipboard));

  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputInjector);
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
