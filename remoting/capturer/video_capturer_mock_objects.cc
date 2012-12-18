// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/video_capturer_mock_objects.h"

#include "remoting/capturer/video_frame_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

MockVideoFrameCapturer::MockVideoFrameCapturer() {}

MockVideoFrameCapturer::~MockVideoFrameCapturer() {}

MockVideoFrameCapturerDelegate::MockVideoFrameCapturerDelegate() {
}

MockVideoFrameCapturerDelegate::~MockVideoFrameCapturerDelegate() {
}

void MockVideoFrameCapturerDelegate::OnCursorShapeChanged(
    scoped_ptr<MouseCursorShape> cursor_shape) {
  // Notify the mock method.
  OnCursorShapeChangedPtr(cursor_shape.get());
}

}  // namespace remoting
