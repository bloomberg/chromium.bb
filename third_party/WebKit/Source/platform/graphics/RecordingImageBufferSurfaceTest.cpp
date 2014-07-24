// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/graphics/RecordingImageBufferSurface.h"

#include "platform/graphics/ImageBuffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace blink;
using testing::Test;

class RecordingImageBufferSurfaceTest : public Test {
protected:
    RecordingImageBufferSurfaceTest()
    {
        OwnPtr<RecordingImageBufferSurface> testSurface = adoptPtr(new RecordingImageBufferSurface(IntSize(10, 10)));
        m_testSurface = testSurface.get();
        // We create an ImageBuffer in order for the testSurface to be
        // properly initialized with a GraphicsContext
        m_imageBuffer = ImageBuffer::create(testSurface.release());
    }

    void testEmptyPicture()
    {
        m_testSurface->initializeCurrentFrame();
        RefPtr<SkPicture> picture = m_testSurface->getPicture();
        EXPECT_TRUE((bool)picture.get());
        expectDisplayListEnabled(true);
    }

    void testNoFallbackWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->didClearCanvas();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }

    void testNonAnimatedCanvasUpdate()
    {
        m_testSurface->initializeCurrentFrame();
        // acquire picture twice to simulate a static canvas: nothing drawn between updates
        m_testSurface->getPicture();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }

    void testAnimatedWithoutClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        m_testSurface->willUse();
        expectDisplayListEnabled(true);
        // This will trigger fallback
        m_testSurface->getPicture();
        expectDisplayListEnabled(false);
    }

    void testAnimatedWithClear()
    {
        m_testSurface->initializeCurrentFrame();
        m_testSurface->getPicture();
        m_testSurface->didClearCanvas();
        m_testSurface->willUse();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
        // clear after use
        m_testSurface->willUse();
        m_testSurface->didClearCanvas();
        m_testSurface->getPicture();
        expectDisplayListEnabled(true);
    }
private:
    void expectDisplayListEnabled(bool displayListEnabled)
    {
        EXPECT_EQ(displayListEnabled, (bool)m_testSurface->m_currentFrame.get());
        EXPECT_EQ(!displayListEnabled, (bool)m_testSurface->m_rasterCanvas.get());
    }

    RecordingImageBufferSurface* m_testSurface;
    OwnPtr<ImageBuffer> m_imageBuffer;
};

namespace {

TEST_F(RecordingImageBufferSurfaceTest, testEmptyPicture)
{
    testEmptyPicture();
}

TEST_F(RecordingImageBufferSurfaceTest, testNoFallbackWithClear)
{
    testNoFallbackWithClear();
}

TEST_F(RecordingImageBufferSurfaceTest, testNonAnimatedCanvasUpdate)
{
    testNonAnimatedCanvasUpdate();
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithoutClear)
{
    testAnimatedWithoutClear();
}

TEST_F(RecordingImageBufferSurfaceTest, testAnimatedWithClear)
{
    testAnimatedWithClear();
}

} // namespace
