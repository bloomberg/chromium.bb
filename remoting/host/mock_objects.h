// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOCK_OBJECTS_H_
#define REMOTING_HOST_MOCK_OBJECTS_H_

#include "remoting/host/capturer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockCapturer : public Capturer {
 public:
  MockCapturer() : Capturer(NULL) {}

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

}  // namespace remoting

#endif  // REMOTING_HOST_MOCK_OBJECTS_H_
