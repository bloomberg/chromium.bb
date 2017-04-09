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
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Test;

namespace blink {

class MockSurfaceFactory : public RecordingImageBufferFallbackSurfaceFactory {
 public:
  MockSurfaceFactory() : create_surface_count_(0) {}

  virtual std::unique_ptr<ImageBufferSurface> CreateSurface(
      const IntSize& size,
      OpacityMode opacity_mode,
      sk_sp<SkColorSpace> color_space,
      SkColorType color_type) {
    create_surface_count_++;
    return WTF::WrapUnique(new UnacceleratedImageBufferSurface(
        size, opacity_mode, kInitializeImagePixels, std::move(color_space),
        color_type));
  }

  virtual ~MockSurfaceFactory() {}

  int CreateSurfaceCount() { return create_surface_count_; }

 private:
  int create_surface_count_;
};

class RecordingImageBufferSurfaceTest : public Test {
 protected:
  RecordingImageBufferSurfaceTest() {
    std::unique_ptr<MockSurfaceFactory> surface_factory =
        WTF::MakeUnique<MockSurfaceFactory>();
    surface_factory_ = surface_factory.get();
    std::unique_ptr<RecordingImageBufferSurface> test_surface =
        WTF::WrapUnique(new RecordingImageBufferSurface(
            IntSize(10, 10), std::move(surface_factory), kNonOpaque, nullptr));
    test_surface_ = test_surface.get();
    // We create an ImageBuffer in order for the testSurface to be
    // properly initialized with a GraphicsContext
    image_buffer_ = ImageBuffer::Create(std::move(test_surface));
    EXPECT_FALSE(!image_buffer_);
    test_surface_->InitializeCurrentFrame();
  }

 public:
  RecordingImageBufferSurface* TestSurface() { return test_surface_; }
  int CreateSurfaceCount() { return surface_factory_->CreateSurfaceCount(); }
  PaintCanvas* Canvas() { return image_buffer_->Canvas(); }

  void ExpectDisplayListEnabled(bool display_list_enabled) {
    EXPECT_EQ(display_list_enabled, (bool)test_surface_->current_frame_.get());
    EXPECT_EQ(!display_list_enabled,
              (bool)test_surface_->fallback_surface_.get());
    int expected_surface_creation_count = display_list_enabled ? 0 : 1;
    EXPECT_EQ(expected_surface_creation_count,
              surface_factory_->CreateSurfaceCount());
  }

 private:
  MockSurfaceFactory* surface_factory_;
  RecordingImageBufferSurface* test_surface_;
  std::unique_ptr<ImageBuffer> image_buffer_;
};

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture) {
  sk_sp<PaintRecord> record = TestSurface()->GetRecord();
  EXPECT_TRUE(record.get());
  ExpectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear) {
  TestSurface()->WillOverwriteCanvas();
  TestSurface()->GetRecord();
  ExpectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testNonAnimatedCanvasUpdate) {
  // Acquire picture twice to simulate a static canvas: nothing drawn between
  // updates.
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  TestSurface()->GetRecord();
  ExpectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithoutClear) {
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  EXPECT_EQ(0, CreateSurfaceCount());
  ExpectDisplayListEnabled(true);  // first frame has an implicit clear
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  ExpectDisplayListEnabled(false);
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithClear) {
  TestSurface()->GetRecord();
  TestSurface()->WillOverwriteCanvas();
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  ExpectDisplayListEnabled(true);
  // clear after use
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->WillOverwriteCanvas();
  TestSurface()->GetRecord();
  ExpectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testClearRect) {
  TestSurface()->GetRecord();
  PaintFlags clear_flags;
  clear_flags.setBlendMode(SkBlendMode::kClear);
  Canvas()->drawRect(SkRect::MakeWH(TestSurface()->size().Width(),
                                    TestSurface()->size().Height()),
                     clear_flags);
  TestSurface()->DidDraw(FloatRect(0, 0, 1, 1));
  TestSurface()->GetRecord();
  ExpectDisplayListEnabled(true);
}

}  // namespace blink
