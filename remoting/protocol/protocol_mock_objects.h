// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_

#include <string>

#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/video_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {
namespace protocol {

class MockConnectionToClient : public ConnectionToClient {
 public:
  MockConnectionToClient(Session* session,
                         HostStub* host_stub,
                         InputStub* input_stub);
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

  MOCK_METHOD1(OnConnectionOpened, void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionClosed, void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionFailed, void(ConnectionToClient* connection));
  MOCK_METHOD2(OnSequenceNumberUpdated, void(ConnectionToClient* connection,
                                             int64 sequence_number));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClientEventHandler);
};

class MockInputStub : public InputStub {
 public:
  MockInputStub();
  virtual ~MockInputStub();

  MOCK_METHOD1(InjectKeyEvent, void(const KeyEvent& event));
  MOCK_METHOD1(InjectMouseEvent, void(const MouseEvent& event));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

class MockHostStub : public HostStub {
 public:
  MockHostStub();
  virtual ~MockHostStub();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostStub);
};

class MockClientStub : public ClientStub {
 public:
  MockClientStub();
  virtual ~MockClientStub();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientStub);
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub();
  virtual ~MockVideoStub();

  MOCK_METHOD2(ProcessVideoPacket, void(const VideoPacket* video_packet,
                                        const base::Closure& done));
  MOCK_METHOD0(GetPendingPackets, int());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoStub);
};

class MockSession : public Session {
 public:
  MockSession();
  virtual ~MockSession();

  MOCK_METHOD1(SetStateChangeCallback,
               void(const StateChangeCallback& callback));
  MOCK_METHOD0(error, Session::Error());
  MOCK_METHOD2(CreateStreamChannel, void(
      const std::string& name, const StreamChannelCallback& callback));
  MOCK_METHOD2(CreateDatagramChannel, void(
      const std::string& name, const DatagramChannelCallback& callback));
  MOCK_METHOD1(CancelChannelCreation, void(const std::string& name));
  MOCK_METHOD0(control_channel, net::Socket*());
  MOCK_METHOD0(event_channel, net::Socket*());
  MOCK_METHOD0(video_channel, net::Socket*());
  MOCK_METHOD0(video_rtp_channel, net::Socket*());
  MOCK_METHOD0(video_rtcp_channel, net::Socket*());
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

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOCOL_MOCK_OBJECTS_H_
