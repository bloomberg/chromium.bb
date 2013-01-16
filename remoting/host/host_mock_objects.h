// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include "net/base/ip_endpoint.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/client_session.h"
#include "remoting/host/continue_window.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/disconnect_window.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/host_status_observer.h"
#include "remoting/host/local_input_monitor.h"
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

  MOCK_METHOD1(CreateAudioCapturerPtr,
               AudioCapturer*(scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD2(CreateEventExecutorPtr,
               EventExecutor*(scoped_refptr<base::SingleThreadTaskRunner>,
                              scoped_refptr<base::SingleThreadTaskRunner>));
  MOCK_METHOD2(
      CreateVideoCapturerPtr,
      VideoFrameCapturer*(scoped_refptr<base::SingleThreadTaskRunner>,
                          scoped_refptr<base::SingleThreadTaskRunner>));

  // DesktopEnvironment implementation.
  virtual scoped_ptr<AudioCapturer> CreateAudioCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner) OVERRIDE;
  virtual scoped_ptr<EventExecutor> CreateEventExecutor(
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) OVERRIDE;
  virtual scoped_ptr<VideoFrameCapturer> CreateVideoCapturer(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) OVERRIDE;
};

class MockDisconnectWindow : public DisconnectWindow {
 public:
  MockDisconnectWindow();
  virtual ~MockDisconnectWindow();

  MOCK_METHOD2(Show, bool(const base::Closure& disconnect_callback,
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

  MOCK_METHOD1(Show, void(
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

  MOCK_METHOD0(CreatePtr, DesktopEnvironment*());
  MOCK_CONST_METHOD0(SupportsAudioCapture, bool());

  virtual scoped_ptr<DesktopEnvironment> Create(
      const std::string& client_jid,
      const base::Closure& disconnect_callback) OVERRIDE;

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
