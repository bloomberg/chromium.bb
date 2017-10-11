// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/RecordingImageBufferSurface.h"

#include <memory>
#include "platform/WebTaskRunner.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Test;

namespace blink {

class RecordingImageBufferSurfaceTest : public Test {
 protected:
  RecordingImageBufferSurfaceTest() {
    auto test_surface = std::make_unique<RecordingImageBufferSurface>(
        IntSize(10, 10), RecordingImageBufferSurface::kAllowFallback);
    test_surface_ = test_surface.get();
    // We create an ImageBuffer in order for the |test_surface| to be
    // properly initialized with a GraphicsContext
    image_buffer_ = ImageBuffer::Create(std::move(test_surface));
    EXPECT_TRUE(image_buffer_);
  }

 public:
  RecordingImageBufferSurface* TestSurface() { return test_surface_; }
  PaintCanvas* Canvas() { return image_buffer_->Canvas(); }

 private:
  RecordingImageBufferSurface* test_surface_;
  std::unique_ptr<ImageBuffer> image_buffer_;
};

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture) {
  sk_sp<PaintRecord> record = TestSurface()->GetRecord();
  EXPECT_TRUE(record.get());
  EXPECT_TRUE(TestSurface()->IsRecording());
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear) {
  TestSurface()->WillOverwriteCanvas();
  TestSurface()->GetRecord();
  EXPECT_TRUE(TestSurface()->IsRecording());
}

TEST_F(RecordingImageBufferSurfaceTest, testNonAnimatedCanvasUpdate) {
  // Acquire picture twice to simulate a static canvas: nothing drawn between
  // updates.
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  TestSurface()->GetRecord();
  EXPECT_TRUE(TestSurface()->IsRecording());
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithoutClear) {
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  EXPECT_TRUE(TestSurface()->IsRecording());
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  EXPECT_FALSE(TestSurface()->IsRecording());
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithClear) {
  TestSurface()->GetRecord();
  TestSurface()->WillOverwriteCanvas();
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  EXPECT_TRUE(TestSurface()->IsRecording());
  // Clear after use.
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->WillOverwriteCanvas();
  TestSurface()->GetRecord();
  EXPECT_TRUE(TestSurface()->IsRecording());
}

TEST_F(RecordingImageBufferSurfaceTest, testClearRect) {
  TestSurface()->GetRecord();
  PaintFlags clear_flags;
  clear_flags.setBlendMode(SkBlendMode::kClear);
  Canvas()->drawRect(SkRect::MakeWH(TestSurface()->Size().Width(),
                                    TestSurface()->Size().Height()),
                     clear_flags);
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  EXPECT_TRUE(TestSurface()->IsRecording());
}

}  // namespace blink
