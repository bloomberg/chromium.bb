// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MOCK_OBJECTS_H_
#define REMOTING_PROTOCOL_MOCK_OBJECTS_H_

#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/video_stub.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {
namespace protocol {

class ChromotocolConnection;

class MockConnectionToClient : public ConnectionToClient {
 public:
  MockConnectionToClient() {}

  MOCK_METHOD1(Init, void(ChromotocolConnection* connection));
  MOCK_METHOD0(video_stub, VideoStub*());
  MOCK_METHOD0(Disconnect, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClient);
};

class MockConnectionToClientEventHandler :
  public ConnectionToClient::EventHandler {
 public:
  MockConnectionToClientEventHandler() {}

  MOCK_METHOD1(OnConnectionOpened, void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionClosed, void(ConnectionToClient* connection));
  MOCK_METHOD1(OnConnectionFailed, void(ConnectionToClient* connection));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionToClientEventHandler);
};

class MockInputStub : public InputStub {
 public:
  MockInputStub() {}

  MOCK_METHOD2(InjectKeyEvent, void(const KeyEvent* event, Task* done));
  MOCK_METHOD2(InjectMouseEvent, void(const MouseEvent* event, Task* done));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputStub);
};

class MockHostStub : public HostStub {
 public:
  MockHostStub() {}

  MOCK_METHOD2(SuggestResolution, void(const SuggestResolutionRequest* msg,
                                       Task* done));
  MOCK_METHOD2(BeginSessionRequest,
               void(const LocalLoginCredentials* credentials, Task* done));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHostStub);
};

class MockVideoStub : public VideoStub {
 public:
  MockVideoStub() {}

  MOCK_METHOD2(ProcessVideoPacket, void(const VideoPacket* video_packet,
                                        Task* done));
  MOCK_METHOD0(GetPendingPackets, int());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MOCK_OBJECTS_H_
