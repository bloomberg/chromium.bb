// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_HOST_MOCK_OBJECTS_H_

#include "remoting/host/capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockCapturer : public Capturer {
 public:
  MockCapturer();
  virtual ~MockCapturer();

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

class MockChromotingHostContext : public ChromotingHostContext {
 public:
  MockChromotingHostContext();
  virtual ~MockChromotingHostContext();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(jingle_thread, JingleThread*());
  MOCK_METHOD0(main_message_loop, MessageLoop*());
  MOCK_METHOD0(encode_message_loop, MessageLoop*());
  MOCK_METHOD0(network_message_loop, MessageLoop*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChromotingHostContext);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_MOCK_OBJECTS_H_
