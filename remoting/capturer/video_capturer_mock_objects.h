// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_VIDEO_CAPTURER_MOCK_OBJECTS_H_
#define REMOTING_CAPTURER_VIDEO_CAPTURER_MOCK_OBJECTS_H_

#include "remoting/capturer/capture_data.h"
#include "remoting/capturer/mouse_cursor_shape.h"
#include "remoting/capturer/video_frame_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockVideoFrameCapturer : public VideoFrameCapturer {
 public:
  MockVideoFrameCapturer();
  virtual ~MockVideoFrameCapturer();

  MOCK_METHOD1(Start, void(Delegate* delegate));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(InvalidateRegion, void(const SkRegion& invalid_region));
  MOCK_METHOD0(CaptureFrame, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameCapturer);
};

class MockVideoFrameCapturerDelegate : public VideoFrameCapturer::Delegate {
 public:
  MockVideoFrameCapturerDelegate();
  virtual ~MockVideoFrameCapturerDelegate();

  void OnCursorShapeChanged(scoped_ptr<MouseCursorShape> cursor_shape) OVERRIDE;

  MOCK_METHOD1(OnCaptureCompleted, void(scoped_refptr<CaptureData>));
  MOCK_METHOD1(OnCursorShapeChangedPtr,
               void(MouseCursorShape* cursor_shape));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoFrameCapturerDelegate);
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_VIDEO_CAPTURER_MOCK_OBJECTS_H_
