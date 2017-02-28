// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/WebTaskRunner.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

using testing::Test;

namespace blink {

class MockSurfaceFactory : public RecordingImageBufferFallbackSurfaceFactory {
 public:
  MockSurfaceFactory() : m_createSurfaceCount(0) {}

  virtual std::unique_ptr<ImageBufferSurface> createSurface(
      const IntSize& size,
      OpacityMode opacityMode,
      sk_sp<SkColorSpace> colorSpace,
      SkColorType colorType) {
    m_createSurfaceCount++;
    return WTF::wrapUnique(new UnacceleratedImageBufferSurface(
        size, opacityMode, InitializeImagePixels, std::move(colorSpace),
        colorType));
  }

  virtual ~MockSurfaceFactory() {}

  int createSurfaceCount() { return m_createSurfaceCount; }

 private:
  int m_createSurfaceCount;
};

class RecordingImageBufferSurfaceTest : public Test {
 protected:
  RecordingImageBufferSurfaceTest() {
    std::unique_ptr<MockSurfaceFactory> surfaceFactory =
        WTF::makeUnique<MockSurfaceFactory>();
    m_surfaceFactory = surfaceFactory.get();
    std::unique_ptr<RecordingImageBufferSurface> testSurface =
        WTF::wrapUnique(new RecordingImageBufferSurface(
            IntSize(10, 10), std::move(surfaceFactory), NonOpaque, nullptr));
    m_testSurface = testSurface.get();
    // We create an ImageBuffer in order for the testSurface to be
    // properly initialized with a GraphicsContext
    m_imageBuffer = ImageBuffer::create(std::move(testSurface));
    EXPECT_FALSE(!m_imageBuffer);
    m_testSurface->initializeCurrentFrame();
  }

 public:
  RecordingImageBufferSurface* testSurface() { return m_testSurface; }
  int createSurfaceCount() { return m_surfaceFactory->createSurfaceCount(); }
  PaintCanvas* canvas() { return m_imageBuffer->canvas(); }

  void expectDisplayListEnabled(bool displayListEnabled) {
    EXPECT_EQ(displayListEnabled, (bool)m_testSurface->m_currentFrame.get());
    EXPECT_EQ(!displayListEnabled,
              (bool)m_testSurface->m_fallbackSurface.get());
    int expectedSurfaceCreationCount = displayListEnabled ? 0 : 1;
    EXPECT_EQ(expectedSurfaceCreationCount,
              m_surfaceFactory->createSurfaceCount());
  }

 private:
  MockSurfaceFactory* m_surfaceFactory;
  RecordingImageBufferSurface* m_testSurface;
  std::unique_ptr<ImageBuffer> m_imageBuffer;
};

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture) {
  sk_sp<PaintRecord> record = testSurface()->getRecord();
  EXPECT_TRUE(record.get());
  expectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear) {
  testSurface()->willOverwriteCanvas();
  testSurface()->getRecord();
  expectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testNonAnimatedCanvasUpdate) {
  // Acquire picture twice to simulate a static canvas: nothing drawn between
  // updates.
  testSurface()->didDraw(FloatRect(0, 0, 1, 1));
  testSurface()->getRecord();
  testSurface()->getRecord();
  expectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithoutClear) {
  testSurface()->didDraw(FloatRect(0, 0, 1, 1));
  testSurface()->getRecord();
  EXPECT_EQ(0, createSurfaceCount());
  expectDisplayListEnabled(true);  // first frame has an implicit clear
  testSurface()->didDraw(FloatRect(0, 0, 1, 1));
  testSurface()->getRecord();
  expectDisplayListEnabled(false);
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithClear) {
  testSurface()->getRecord();
  testSurface()->willOverwriteCanvas();
  testSurface()->didDraw(FloatRect(0, 0, 1, 1));
  testSurface()->getRecord();
  expectDisplayListEnabled(true);
  // clear after use
  testSurface()->didDraw(FloatRect(0, 0, 1, 1));
  testSurface()->willOverwriteCanvas();
  testSurface()->getRecord();
  expectDisplayListEnabled(true);
}

TEST_F(RecordingImageBufferSurfaceTest, testClearRect) {
  testSurface()->getRecord();
  PaintFlags clearFlags;
  clearFlags.setBlendMode(SkBlendMode::kClear);
  canvas()->drawRect(SkRect::MakeWH(testSurface()->size().width(),
                                    testSurface()->size().height()),
                     clearFlags);
  testSurface()->didDraw(FloatRect(0, 0, 1, 1));
  testSurface()->getRecord();
  expectDisplayListEnabled(true);
}

}  // namespace blink
