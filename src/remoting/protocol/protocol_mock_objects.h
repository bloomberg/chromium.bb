// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/channel_authenticator.h"
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
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

class VideoEncoder;

namespace protocol {

class MockAuthenticator : public Authenticator {
 public:
  MockAuthenticator();
  ~MockAuthenticator() override;

  MOCK_CONST_METHOD0(state, Authenticator::State());
  MOCK_CONST_METHOD0(started, bool());
  MOCK_CONST_METHOD0(rejection_reason, Authenticator::RejectionReason());
  MOCK_CONST_METHOD0(GetAuthKey, const std::string&());
  MOCK_CONST_METHOD0(CreateChannelAuthenticatorPtr, ChannelAuthenticator*());
  MOCK_METHOD2(ProcessMessage,
               void(const jingle_xmpp::XmlElement* message,
                    base::OnceClosure resume_callback));
  MOCK_METHOD0(GetNextMessagePtr, jingle_xmpp::XmlElement*());

  std::unique_ptr<ChannelAuthenticator> CreateChannelAuthenticator()
      const override {
    return base::WrapUnique(CreateChannelAuthenticatorPtr());
  }

  std::unique_ptr<jingle_xmpp::XmlElement> GetNextMessage() override {
    return base::WrapUnique(GetNextMessagePtr());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAuthenticator);
};

class MockConnectionToClientEventHandler
    : public ConnectionToClient::EventHandler {
 public:
  MockConnectionToClientEventHandler();
  ~MockConnectionToClientEventHandler() override;

  MOCK_METHOD0(OnConnectionAuthenticating, void());
  MOCK_METHOD0(OnConnectionAuthenticated, void());
  MOCK_METHOD0(CreateMediaStreams, void());
  MOCK_METHOD0(OnConnectionChannelsConnected, void());
  MOCK_METHOD1(OnConnectionClosed, void(ErrorCode error));
  MOCK_METHOD1(OnTransportProtocolChange, void(const std::string& protocol));
  MOCK_METHOD1(OnCreateVideoEncoder,
               void(std::unique_ptr<VideoEncoder>* encoder));
  MOCK_METHOD2(OnRouteChange,
               void(const std::string& channel_name,
                    const TransportRoute& route));

  MOCK_METHOD2(OnIncomingDataChannelPtr,
               void(const std::string& channel_name, MessagePipe* pipe));
  void OnIncomingDataChannel(const std::string& channel_name,
                             std::unique_ptr<MessagePipe> pipe) override {
    OnIncomingDataChannelPtr(channel_name, pipe.get());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClientEventHandler);
};

class MockClipboardStub : public ClipboardStub {
 public:
  MockClipboardStub();
  ~MockClipboardStub() override;

  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClipboardStub);
};

class MockInputStub : public InputStub {
 public:
  MockInputStub();
  ~MockInputStub() override;

  MOCK_METHOD1(InjectKeyEvent, void(const KeyEvent& event));
  MOCK_METHOD1(InjectTextEvent, void(const TextEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const MouseEvent& event));
  MOCK_METHOD1(InjectTouchEvent, void(const TouchEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

class MockHostStub : public HostStub {
 public:
  MockHostStub();
  ~MockHostStub() override;

  MOCK_METHOD1(NotifyClientResolution,
               void(const ClientResolution& resolution));
  MOCK_METHOD1(ControlVideo, void(const VideoControl& video_control));
  MOCK_METHOD1(ControlAudio, void(const AudioControl& audio_control));
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(RequestPairing, void(const PairingRequest& pairing_request));
  MOCK_METHOD1(DeliverClientMessage, void(const ExtensionMessage& message));
  MOCK_METHOD1(SelectDesktopDisplay,
               void(const SelectDesktopDisplayRequest& message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostStub);
};

class MockClientStub : public ClientStub {
 public:
  MockClientStub();
  ~MockClientStub() override;

  // ClientStub mock implementation.
  MOCK_METHOD1(SetCapabilities, void(const Capabilities& capabilities));
  MOCK_METHOD1(SetPairingResponse,
               void(const PairingResponse& pairing_response));
  MOCK_METHOD1(DeliverHostMessage, void(const ExtensionMessage& message));
  MOCK_METHOD1(SetVideoLayout, void(const VideoLayout& layout));
  MOCK_METHOD1(SetTransportInfo, void(const TransportInfo& transport_info));

  // ClipboardStub mock implementation.
  MOCK_METHOD1(InjectClipboardEvent, void(const ClipboardEvent& event));

  // CursorShapeStub mock implementation.
  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));

  // KeyboardLayoutStub mock implementation.
  MOCK_METHOD1(SetKeyboardLayout, void(const KeyboardLayout& layout));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientStub);
};

class MockCursorShapeStub : public CursorShapeStub {
 public:
  MockCursorShapeStub();
  ~MockCursorShapeStub() override;

  MOCK_METHOD1(SetCursorShape, void(const CursorShapeInfo& cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCursorShapeStub);
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub();
  ~MockVideoStub() override;

  MOCK_METHOD2(ProcessVideoPacketPtr,
               void(const VideoPacket* video_packet, base::OnceClosure* done));
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> video_packet,
                          base::OnceClosure done) override {
    ProcessVideoPacketPtr(video_packet.get(), &done);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoStub);
};

class MockSession : public Session {
 public:
  MockSession();
  ~MockSession() override;

  MOCK_METHOD1(SetEventHandler, void(Session::EventHandler* event_handler));
  MOCK_METHOD0(error, ErrorCode());
  MOCK_METHOD1(SetTransport, void(Transport*));
  MOCK_METHOD0(jid, const std::string&());
  MOCK_METHOD0(config, const SessionConfig&());
  MOCK_METHOD1(Close, void(ErrorCode error));
  MOCK_METHOD1(AddPlugin, void(SessionPlugin* plugin));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSession);
};

class MockSessionManager : public SessionManager {
 public:
  MockSessionManager();
  ~MockSessionManager() override;

  MOCK_METHOD1(AcceptIncoming, void(const IncomingSessionCallback&));
  void set_protocol_config(
      std::unique_ptr<CandidateSessionConfig> config) override {}
  MOCK_METHOD2(ConnectPtr,
               Session*(const SignalingAddress& peer_address,
                        Authenticator* authenticator));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(set_authenticator_factory_ptr,
               void(AuthenticatorFactory* factory));
  std::unique_ptr<Session> Connect(
      const SignalingAddress& peer_address,
      std::unique_ptr<Authenticator> authenticator) override {
    return base::WrapUnique(ConnectPtr(peer_address, authenticator.get()));
  }
  void set_authenticator_factory(
      std::unique_ptr<AuthenticatorFactory> authenticator_factory) override {
    set_authenticator_factory_ptr(authenticator_factory.release());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSessionManager);
};

// Simple delegate that caches information on paired clients in memory.
class MockPairingRegistryDelegate : public PairingRegistry::Delegate {
 public:
  MockPairingRegistryDelegate();
  ~MockPairingRegistryDelegate() override;

  // PairingRegistry::Delegate implementation.
  std::unique_ptr<base::ListValue> LoadAll() override;
  bool DeleteAll() override;
  protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) override;
  bool Save(const protocol::PairingRegistry::Pairing& pairing) override;
  bool Delete(const std::string& client_id) override;

 private:
  typedef std::map<std::string, protocol::PairingRegistry::Pairing> Pairings;
  Pairings pairings_;
};

class SynchronousPairingRegistry : public PairingRegistry {
 public:
  explicit SynchronousPairingRegistry(std::unique_ptr<Delegate> delegate);

 protected:
  ~SynchronousPairingRegistry() override;

  // Runs tasks synchronously instead of posting them to |task_runner|.
  void PostTask(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                const base::Location& from_here,
                base::OnceClosure task) override;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
