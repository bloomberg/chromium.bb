// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_MOCK_OBJECTS_H_

#include "media/base/data_buffer.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/host/capturer.h"
#include "remoting/host/client_connection.h"
#include "remoting/host/encoder.h"
#include "remoting/host/event_executor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockCapturer : public Capturer {
 public:
  MockCapturer() {}

  MOCK_METHOD1(CaptureFullScreen, void(Task* done_task));
  MOCK_METHOD1(CaptureDirtyRects, void(Task* done_task));
  MOCK_METHOD2(CaptureRect, void(const gfx::Rect& rect, Task* done_task));
  MOCK_CONST_METHOD1(GetData, void(const uint8* planes[]));
  MOCK_CONST_METHOD1(GetDataStride, void(int strides[]));
  MOCK_CONST_METHOD1(GetDirtyRects, void(DirtyRects* rects));
  MOCK_CONST_METHOD0(GetWidth, int());
  MOCK_CONST_METHOD0(GetHeight, int());
  MOCK_CONST_METHOD0(GetPixelFormat, chromotocol_pb::PixelFormat());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCapturer);
};

class MockEncoder : public Encoder {
 public:
  MockEncoder() {}

  MOCK_METHOD8(Encode, void(
      const DirtyRects& dirty_rects,
      const uint8** planes,
      const int* strides,
      bool key_frame,
      chromotocol_pb::UpdateStreamPacketHeader* output_data_header,
      scoped_refptr<media::DataBuffer>* output_data,
      bool* encode_done,
      Task* data_available_task));
  MOCK_METHOD2(SetSize, void(int width, int height));
  MOCK_METHOD1(SetPixelFormat, void(chromotocol_pb::PixelFormat pixel_format));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEncoder);
};

class MockEventExecutor : public EventExecutor {
 public:
  MockEventExecutor() {}

  MOCK_METHOD1(HandleInputEvents, void(ClientMessageList* messages));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEventExecutor);
};

class MockClientConnection : public ClientConnection {
 public:
  MockClientConnection(){}

  MOCK_METHOD2(SendInitClientMessage, void(int width, int height));
  MOCK_METHOD0(SendBeginUpdateStreamMessage, void());
  MOCK_METHOD2(SendUpdateStreamPacketMessage,
               void(chromotocol_pb::UpdateStreamPacketHeader* header,
                    scoped_refptr<media::DataBuffer> data));
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
