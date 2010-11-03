// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_MOCK_OBJECTS_H_

#include "remoting/host/capturer.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/event_executor.h"
#include "remoting/proto/internal.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockCapturer : public Capturer {
 public:
  MockCapturer() {}

  MOCK_METHOD0(ScreenConfigurationChanged, void());
  MOCK_METHOD1(InvalidateRects, void(const InvalidRects& inval_rects));
  MOCK_METHOD0(InvalidateFullScreen, void());
  MOCK_METHOD0(CalculateInvalidRects, void());
  MOCK_METHOD1(CaptureInvalidRects, void(CaptureCompletedCallback* callback));
  MOCK_METHOD2(CaptureRects, void(const InvalidRects& rects,
                                  CaptureCompletedCallback* callback));
  MOCK_CONST_METHOD0(width, int());
  MOCK_CONST_METHOD0(height, int());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCapturer);
};

class MockEventExecutor : public EventExecutor {
 public:
   MockEventExecutor(Capturer* capturer) : EventExecutor(capturer) {}

  MOCK_METHOD1(HandleInputEvent, void(ChromotingClientMessage* messages));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventExecutor);
};

class MockClientConnection : public ClientConnection {
 public:
  MockClientConnection(){}

  MOCK_METHOD1(Init, void(ChromotocolConnection* connection));
  MOCK_METHOD2(SendInitClientMessage, void(int width, int height));
  MOCK_METHOD1(SendVideoPacket, void(const VideoPacket& packet));
  MOCK_METHOD0(GetPendingUpdateStreamMessages, int());
  MOCK_METHOD0(Disconnect, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientConnection);
};

class MockClientConnectionEventHandler : public ClientConnection::EventHandler {
 public:
  MockClientConnectionEventHandler() {}

  MOCK_METHOD2(HandleMessage,
               void(ClientConnection* viewer,
                    ChromotingClientMessage* message));
  MOCK_METHOD1(OnConnectionOpened, void(ClientConnection* viewer));
  MOCK_METHOD1(OnConnectionClosed, void(ClientConnection* viewer));
  MOCK_METHOD1(OnConnectionFailed, void(ClientConnection* viewer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientConnectionEventHandler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOCK_OBJECTS_H_
