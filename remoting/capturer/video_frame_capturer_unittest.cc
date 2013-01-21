// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/video_frame_capturer.h"

#include "base/bind.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif  // defined(OS_MACOSX)
#include "remoting/capturer/capture_data.h"
#include "remoting/capturer/shared_buffer_factory.h"
#include "remoting/capturer/video_capturer_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace remoting {

class MockSharedBufferFactory : public SharedBufferFactory {
 public:
  MockSharedBufferFactory() {}
  virtual ~MockSharedBufferFactory() {}

  MOCK_METHOD1(CreateSharedBuffer, scoped_refptr<SharedBuffer>(uint32));
  MOCK_METHOD1(ReleaseSharedBuffer, void(scoped_refptr<SharedBuffer>));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSharedBufferFactory);
};

MATCHER(DirtyRegionIsNonEmptyRect, "") {
  const SkRegion& dirty_region = arg->dirty_region();
  const SkIRect& dirty_region_bounds = dirty_region.getBounds();
  if (dirty_region_bounds.isEmpty()) {
    return false;
  }
  return dirty_region == SkRegion(dirty_region_bounds);
}

class VideoFrameCapturerTest : public testing::Test {
 public:
  scoped_refptr<SharedBuffer> CreateSharedBuffer(uint32 size);

 protected:
  scoped_ptr<VideoFrameCapturer> capturer_;
  MockSharedBufferFactory shared_buffer_factory_;
  MockVideoFrameCapturerDelegate delegate_;
};

scoped_refptr<SharedBuffer> VideoFrameCapturerTest::CreateSharedBuffer(
    uint32 size) {
  return scoped_refptr<SharedBuffer>(new SharedBuffer(size));
}

TEST_F(VideoFrameCapturerTest, StartCapturer) {
  capturer_ = VideoFrameCapturer::Create();
  capturer_->Start(&delegate_);
  capturer_->Stop();
}

#if defined(THREAD_SANITIZER)
// ThreadSanitizer v2 reports a use-after-free, see http://crbug.com/163641.
#define MAYBE_Capture DISABLED_Capture
#else
#define MAYBE_Capture Capture
#endif
TEST_F(VideoFrameCapturerTest, MAYBE_Capture) {
  // Assume that Start() treats the screen as invalid initially.
  EXPECT_CALL(delegate_,
              OnCaptureCompleted(DirtyRegionIsNonEmptyRect()));
  EXPECT_CALL(delegate_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  capturer_ = VideoFrameCapturer::Create();
  capturer_->Start(&delegate_);
  capturer_->CaptureFrame();
  capturer_->Stop();
}

#if defined(OS_WIN)

TEST_F(VideoFrameCapturerTest, UseSharedBuffers) {
  EXPECT_CALL(delegate_,
              OnCaptureCompleted(DirtyRegionIsNonEmptyRect()));
  EXPECT_CALL(delegate_, OnCursorShapeChangedPtr(_))
      .Times(AnyNumber());

  EXPECT_CALL(shared_buffer_factory_, CreateSharedBuffer(_))
      .Times(1)
      .WillOnce(Invoke(this, &VideoFrameCapturerTest::CreateSharedBuffer));
  EXPECT_CALL(shared_buffer_factory_, ReleaseSharedBuffer(_))
      .Times(1);

  capturer_ = VideoFrameCapturer::CreateWithFactory(&shared_buffer_factory_);
  capturer_->Start(&delegate_);
  capturer_->CaptureFrame();
  capturer_->Stop();
  capturer_.reset();
}

#endif  // defined(OS_WIN)

}  // namespace remoting
