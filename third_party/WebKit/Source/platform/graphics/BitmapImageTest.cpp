/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/BitmapImage.h"

#include "platform/SharedBuffer.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class BitmapImageTest : public ::testing::Test {
public:
    class FakeImageObserver : public ImageObserver {
    public:
        FakeImageObserver() : m_lastDecodedSizeChangedDelta(0) { }

        virtual void decodedSizeChanged(const Image*, int delta)
        {
            m_lastDecodedSizeChangedDelta = delta;
        }
        void didDraw(const Image*) override { }
        bool shouldPauseAnimation(const Image*) override { return false; }
        void animationAdvanced(const Image*) override { }

        virtual void changedInRect(const Image*, const IntRect&) { }

        int m_lastDecodedSizeChangedDelta;
    };

    BitmapImageTest() : BitmapImageTest(false) { }
    BitmapImageTest(bool enableDeferredDecoding) : m_enableDeferredDecoding(enableDeferredDecoding) { }

    static PassRefPtr<SharedBuffer> readFile(const char* fileName)
    {
        String filePath = testing::blinkRootDir();
        filePath.append(fileName);
        return testing::readFromFile(filePath);
    }

    // Accessors to BitmapImage's protected methods.
    void destroyDecodedData(bool destroyAll) { m_image->destroyDecodedData(destroyAll); }
    size_t frameCount() { return m_image->frameCount(); }
    void frameAtIndex(size_t index)
    {
        m_image->frameAtIndex(index);
    }
    void setCurrentFrame(size_t frame) { m_image->m_currentFrame = frame; }
    size_t frameDecodedSize(size_t frame) { return m_image->m_frames[frame].m_frameBytes; }
    size_t decodedFramesCount() const { return m_image->m_frames.size(); }

    void setFirstFrameNotComplete() { m_image->m_frames[0].m_isComplete = false; }

    void loadImage(const char* fileName, bool loadAllFrames = true)
    {
        RefPtr<SharedBuffer> imageData = readFile(fileName);
        ASSERT_TRUE(imageData.get());

        m_image->setData(imageData, true);
        EXPECT_EQ(0u, decodedSize());

        size_t frameCount = m_image->frameCount();
        if (loadAllFrames) {
            for (size_t i = 0; i < frameCount; ++i)
                frameAtIndex(i);
        }
    }

    size_t decodedSize()
    {
        // In the context of this test, the following loop will give the correct result, but only because the test
        // forces all frames to be decoded in loadImage() above. There is no general guarantee that frameDecodedSize()
        // is up-to-date. Because of how multi frame images (like GIF) work, requesting one frame to be decoded may
        // require other previous frames to be decoded as well. In those cases frameDecodedSize() wouldn't return the
        // correct thing for the previous frame because the decoded size wouldn't have propagated upwards to the
        // BitmapImage frame cache.
        size_t size = 0;
        for (size_t i = 0; i < decodedFramesCount(); ++i)
            size += frameDecodedSize(i);
        return size;
    }

    void advanceAnimation()
    {
        m_image->advanceAnimation(0);
    }

    PassRefPtr<Image> imageForDefaultFrame()
    {
        return m_image->imageForDefaultFrame();
    }

protected:
    void SetUp() override
    {
        DeferredImageDecoder::setEnabled(m_enableDeferredDecoding);
        m_image = BitmapImage::create(&m_imageObserver);
    }

    FakeImageObserver m_imageObserver;
    RefPtr<BitmapImage> m_image;

private:
    bool m_enableDeferredDecoding;
};

TEST_F(BitmapImageTest, destroyDecodedDataExceptCurrentFrame)
{
    loadImage("/LayoutTests/fast/images/resources/animated-10color.gif");
    size_t totalSize = decodedSize();
    size_t frame = frameCount() / 2;
    setCurrentFrame(frame);
    size_t size = frameDecodedSize(frame);
    destroyDecodedData(false);
    EXPECT_LT(m_imageObserver.m_lastDecodedSizeChangedDelta, 0);
    EXPECT_GE(m_imageObserver.m_lastDecodedSizeChangedDelta, -static_cast<int>(totalSize - size));
}

TEST_F(BitmapImageTest, destroyAllDecodedData)
{
    loadImage("/LayoutTests/fast/images/resources/animated-10color.gif");
    size_t totalSize = decodedSize();
    EXPECT_GT(totalSize, 0u);
    destroyDecodedData(true);
    EXPECT_EQ(-static_cast<int>(totalSize), m_imageObserver.m_lastDecodedSizeChangedDelta);
    EXPECT_EQ(0u, decodedSize());
}

TEST_F(BitmapImageTest, maybeAnimated)
{
    loadImage("/LayoutTests/fast/images/resources/gif-loop-count.gif");
    for (size_t i = 0; i < frameCount(); ++i) {
        EXPECT_TRUE(m_image->maybeAnimated());
        advanceAnimation();
    }
    EXPECT_FALSE(m_image->maybeAnimated());
}

TEST_F(BitmapImageTest, isAllDataReceived)
{
    RefPtr<SharedBuffer> imageData = readFile("/LayoutTests/fast/images/resources/green.jpg");
    ASSERT_TRUE(imageData.get());

    RefPtr<BitmapImage> image = BitmapImage::create();
    EXPECT_FALSE(image->isAllDataReceived());

    image->setData(imageData, false);
    EXPECT_FALSE(image->isAllDataReceived());

    image->setData(imageData, true);
    EXPECT_TRUE(image->isAllDataReceived());

    image->setData(SharedBuffer::create("data", sizeof("data")), false);
    EXPECT_FALSE(image->isAllDataReceived());

    image->setData(imageData, true);
    EXPECT_TRUE(image->isAllDataReceived());
}

TEST_F(BitmapImageTest, noColorProfile)
{
    loadImage("/LayoutTests/fast/images/resources/green.jpg");
    EXPECT_EQ(1u, decodedFramesCount());
    EXPECT_EQ(1024u, decodedSize());
    EXPECT_FALSE(m_image->hasColorProfile());
}

#if USE(QCMSLIB)

TEST_F(BitmapImageTest, jpegHasColorProfile)
{
    loadImage("/LayoutTests/fast/images/resources/icc-v2-gbr.jpg");
    EXPECT_EQ(1u, decodedFramesCount());
    EXPECT_EQ(227700u, decodedSize());
    EXPECT_TRUE(m_image->hasColorProfile());
}

TEST_F(BitmapImageTest, pngHasColorProfile)
{
    loadImage("/LayoutTests/fast/images/resources/palatted-color-png-gamma-one-color-profile.png");
    EXPECT_EQ(1u, decodedFramesCount());
    EXPECT_EQ(65536u, decodedSize());
    EXPECT_TRUE(m_image->hasColorProfile());
}

TEST_F(BitmapImageTest, webpHasColorProfile)
{
    loadImage("/LayoutTests/fast/images/resources/webp-color-profile-lossy.webp");
    EXPECT_EQ(1u, decodedFramesCount());
    EXPECT_EQ(2560000u, decodedSize());
    EXPECT_TRUE(m_image->hasColorProfile());
}

#endif // USE(QCMSLIB)

TEST_F(BitmapImageTest, icoHasWrongFrameDimensions)
{
    loadImage("/LayoutTests/fast/images/resources/wrong-frame-dimensions.ico");
    // This call would cause crash without fix for 408026
    imageForDefaultFrame();
}

TEST_F(BitmapImageTest, correctDecodedDataSize)
{
    // When requesting a frame of a multi-frame GIF causes another frame to be
    // decoded as well, both frames' sizes should be reported by the source and
    // thus included in the decoded size changed notification.
    loadImage("/LayoutTests/fast/images/resources/anim_none.gif", false);
    frameAtIndex(1);
    int frameSize = static_cast<int>(m_image->size().area() * sizeof(ImageFrame::PixelData));
    EXPECT_EQ(frameSize * 2, m_imageObserver.m_lastDecodedSizeChangedDelta);

    // Trying to destroy all data except an undecoded frame should cause the
    // decoder to seek backwards and preserve the most recent previous frame
    // necessary to decode that undecoded frame, and destroy all other frames.
    setCurrentFrame(2);
    destroyDecodedData(false);
    EXPECT_EQ(-frameSize, m_imageObserver.m_lastDecodedSizeChangedDelta);
}

TEST_F(BitmapImageTest, recachingFrameAfterDataChanged)
{
    loadImage("/LayoutTests/fast/images/resources/green.jpg");
    setFirstFrameNotComplete();
    EXPECT_GT(m_imageObserver.m_lastDecodedSizeChangedDelta, 0);
    m_imageObserver.m_lastDecodedSizeChangedDelta = 0;

    // Calling dataChanged causes the cache to flush, but doesn't affect the
    // source's decoded frames. It shouldn't affect decoded size.
    m_image->dataChanged(true);
    EXPECT_EQ(0, m_imageObserver.m_lastDecodedSizeChangedDelta);
    // Recaching the first frame also shouldn't affect decoded size.
    m_image->imageForCurrentFrame();
    EXPECT_EQ(0, m_imageObserver.m_lastDecodedSizeChangedDelta);
}

class BitmapImageDeferredDecodingTest : public BitmapImageTest {
public:
    BitmapImageDeferredDecodingTest() : BitmapImageTest(true) { }
};

TEST_F(BitmapImageDeferredDecodingTest, correctDecodedDataSize)
{
    // When deferred decoding is enabled, requesting any one frame shouldn't
    // result in decoding any other frames.
    loadImage("/LayoutTests/fast/images/resources/anim_none.gif", false);
    frameAtIndex(1);
    int frameSize = static_cast<int>(m_image->size().area() * sizeof(ImageFrame::PixelData));
    EXPECT_EQ(frameSize, m_imageObserver.m_lastDecodedSizeChangedDelta);
    frameAtIndex(0);

    // Trying to destroy all data except an undecoded frame should go ahead and
    // destroy all other frames.
    setCurrentFrame(2);
    destroyDecodedData(false);
    EXPECT_EQ(-frameSize * 2, m_imageObserver.m_lastDecodedSizeChangedDelta);
}

} // namespace blink
