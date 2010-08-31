// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_MOCK_OBJECTS_H_

#include "media/base/data_buffer.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/host/capturer.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/event_executor.h"
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

  MOCK_METHOD1(HandleInputEvents, void(ClientMessageList* messages));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventExecutor);
};

class MockClientConnection : public ClientConnection {
 public:
  MockClientConnection(){}

  MOCK_METHOD2(SendInitClientMessage, void(int width, int height));
  MOCK_METHOD0(SendBeginUpdateStreamMessage, void());
  MOCK_METHOD1(SendUpdateStreamPacketMessage,
               void(scoped_refptr<media::DataBuffer> data));
  MOCK_METHOD0(SendEndUpdateStreamMessage, void());
  MOCK_METHOD0(GetPendingUpdateStreamMessages, int());

  MOCK_METHOD2(OnStateChange, void(JingleChannel* channel,
                                   JingleChannel::State state));
  MOCK_METHOD2(OnPacketReceived, void(JingleChannel* channel,
                                      scoped_refptr<media::DataBuffer> data));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientConnection);
};

class MockClientConnectionEventHandler : public ClientConnection::EventHandler {
 public:
  MockClientConnectionEventHandler() {}

  MOCK_METHOD2(HandleMessages,
               void(ClientConnection* viewer, ClientMessageList* messages));
  MOCK_METHOD1(OnConnectionOpened, void(ClientConnection* viewer));
  MOCK_METHOD1(OnConnectionClosed, void(ClientConnection* viewer));
  MOCK_METHOD1(OnConnectionFailed, void(ClientConnection* viewer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockClientConnectionEventHandler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOCK_OBJECTS_H_
