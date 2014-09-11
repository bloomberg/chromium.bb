// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_

#include <map>
#include <string>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/pairing_registry.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_manager.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/video_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {
namespace protocol {

class MockConnectionToClient : public ConnectionToClient {
 public:
  MockConnectionToClient(Session* session,
                         HostStub* host_stub);
  virtual ~MockConnectionToClient();

  MOCK_METHOD1(Init, void(Session* session));
  MOCK_METHOD0(video_stub, VideoStub*());
  MOCK_METHOD0(client_stub, ClientStub*());
  MOCK_METHOD0(session, Session*());
  MOCK_METHOD0(Disconnect, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClient);
};

class MockConnectionToClientEventHandler :
      public ConnectionToClient::EventHandler {
 public:
  MockConnectionToClientEventHandler();
  virtual ~MockConnectionToClientEventHandler();

  MOCK_METHOD1(OnConnectionAuthenticating,
               void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionAuthenticated, void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionChannelsConnected,
               void(ConnectionToClient* connection));
  MOCK_METHOD2(OnConnectionClosed, void(ConnectionToClient* connection,
                                        ErrorCode error));
  MOCK_METHOD2(OnSequenceNumberUpdated, void(ConnectionToClient* connection,
                                             int64 sequence_number));
  MOCK_METHOD3(OnRouteChange, void(ConnectionToClient* connection,
                                   const std::string& channel_name,
                                   const TransportRoute& route));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClientEventHandler);
};

class MockClipboardStub : public ClipboardStub {
 public:
  MockClipboardStub();
  virtual ~MockClipboardStub();

  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClipboardStub);
};

class MockCursorShapeChangeCallback {
 public:
  MockCursorShapeChangeCallback();
  virtual ~MockCursorShapeChangeCallback();

  MOCK_METHOD1(CursorShapeChangedPtr, void(CursorShapeInfo* info));
  void CursorShapeChanged(scoped_ptr<CursorShapeInfo> info);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCursorShapeChangeCallback);
};

class MockInputStub : public InputStub {
 public:
  MockInputStub();
  virtual ~MockInputStub();

  MOCK_METHOD1(InjectKeyEvent, void(const KeyEvent& event));
  MOCK_METHOD1(InjectTextEvent, void(const TextEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const MouseEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

class MockHostStub : public HostStub {
 public:
  MockHostStub();
  virtual ~MockHostStub();

  MOCK_METHOD1(NotifyClientResolution,
               void(const ClientResolution& resolution));
  MOCK_METHOD1(ControlVideo, void(const VideoControl& video_control));
  MOCK_METHOD1(ControlAudio, void(const AudioControl& audio_control));
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(RequestPairing,
               void(const PairingRequest& pairing_request));
  MOCK_METHOD1(DeliverClientMessage, void(const ExtensionMessage& message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostStub);
};

class MockClientStub : public ClientStub {
 public:
  MockClientStub();
  virtual ~MockClientStub();

  // ClientStub mock implementation.
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(SetPairingResponse,
               void(const PairingResponse& pairing_response));
  MOCK_METHOD1(DeliverHostMessage, void(const ExtensionMessage& message));

  // ClipboardStub mock implementation.
  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

  // CursorShapeStub mock implementation.
  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientStub);
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub();
  virtual ~MockVideoStub();

  MOCK_METHOD2(ProcessVideoPacketPtr, void(const VideoPacket* video_packet,
                                           const base::Closure& done));
  virtual void ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                                  const base::Closure& done) {
    ProcessVideoPacketPtr(video_packet.get(), done);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoStub);
};

class MockSession : public Session {
 public:
  MockSession();
  virtual ~MockSession();

  MOCK_METHOD1(SetEventHandler, void(Session::EventHandler* event_handler));
  MOCK_METHOD0(error, ErrorCode());
  MOCK_METHOD0(GetTransportChannelFactory, StreamChannelFactory*());
  MOCK_METHOD0(GetMultiplexedChannelFactory, StreamChannelFactory*());
  MOCK_METHOD0(jid, const std::string&());
  MOCK_METHOD0(candidate_config, const CandidateSessionConfig*());
  MOCK_METHOD0(config, const SessionConfig&());
  MOCK_METHOD1(set_config, void(const SessionConfig& config));
  MOCK_METHOD0(initiator_token, const std::string&());
  MOCK_METHOD1(set_initiator_token, void(const std::string& initiator_token));
  MOCK_METHOD0(receiver_token, const std::string&());
  MOCK_METHOD1(set_receiver_token, void(const std::string& receiver_token));
  MOCK_METHOD1(set_shared_secret, void(const std::string& secret));
  MOCK_METHOD0(shared_secret, const std::string&());
  MOCK_METHOD0(Close, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSession);
};

class MockSessionManager : public SessionManager {
 public:
  MockSessionManager();
  virtual ~MockSessionManager();

  MOCK_METHOD2(Init, void(SignalStrategy*, Listener*));
  MOCK_METHOD3(ConnectPtr, Session*(
      const std::string& host_jid,
      Authenticator* authenticator,
      CandidateSessionConfig* config));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(set_authenticator_factory_ptr,
               void(AuthenticatorFactory* factory));
  virtual scoped_ptr<Session> Connect(
      const std::string& host_jid,
      scoped_ptr<Authenticator> authenticator,
      scoped_ptr<CandidateSessionConfig> config) {
    return scoped_ptr<Session>(ConnectPtr(
        host_jid, authenticator.get(), config.get()));
  }
  virtual void set_authenticator_factory(
      scoped_ptr<AuthenticatorFactory> authenticator_factory) {
    set_authenticator_factory_ptr(authenticator_factory.release());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSessionManager);
};

// Simple delegate that caches information on paired clients in memory.
class MockPairingRegistryDelegate : public PairingRegistry::Delegate {
 public:
  MockPairingRegistryDelegate();
  virtual ~MockPairingRegistryDelegate();

  // PairingRegistry::Delegate implementation.
  virtual scoped_ptr<base::ListValue> LoadAll() OVERRIDE;
  virtual bool DeleteAll() OVERRIDE;
  virtual protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) OVERRIDE;
  virtual bool Save(const protocol::PairingRegistry::Pairing& pairing) OVERRIDE;
  virtual bool Delete(const std::string& client_id) OVERRIDE;

 private:
  typedef std::map<std::string, protocol::PairingRegistry::Pairing> Pairings;
  Pairings pairings_;
};

class SynchronousPairingRegistry : public PairingRegistry {
 public:
  explicit SynchronousPairingRegistry(scoped_ptr<Delegate> delegate);

 protected:
  virtual ~SynchronousPairingRegistry();

  // Runs tasks synchronously instead of posting them to |task_runner|.
  virtual void PostTask(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
