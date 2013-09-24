/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/chromium/ImageFrameGenerator.h"

#include "core/platform/SharedBuffer.h"
#include "core/platform/graphics/chromium/ImageDecodingStore.h"
#include "core/platform/graphics/chromium/test/MockImageDecoder.h"
#include "wtf/Threading.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class ImageFrameGeneratorTest;

// Helper methods to generate standard sizes.
SkISize fullSize() { return SkISize::Make(100, 100); }
SkISize scaledSize() { return SkISize::Make(50, 50); }

class ImageFrameGeneratorTest;

class ImageFrameGeneratorTest : public ::testing::Test, public MockImageDecoderClient {
public:
    virtual void SetUp() OVERRIDE
    {
        ImageDecodingStore::initializeOnce();
        m_data = SharedBuffer::create();
        m_generator = ImageFrameGenerator::create(fullSize(), m_data, true);
        m_generator->setImageDecoderFactoryForTesting(MockImageDecoderFactory::create(this, fullSize()));
        m_decodersDestroyed = 0;
        m_frameBufferRequestCount = 0;
        m_status = ImageFrame::FrameEmpty;
    }

    virtual void TearDown() OVERRIDE
    {
        ImageDecodingStore::shutdown();
    }

    virtual void decoderBeingDestroyed() OVERRIDE
    {
        ++m_decodersDestroyed;
    }

    virtual void frameBufferRequested() OVERRIDE
    {
        ++m_frameBufferRequestCount;
    }

    virtual ImageFrame::Status status() OVERRIDE
    {
        ImageFrame::Status currentStatus = m_status;
        m_status = m_nextFrameStatus;
        return currentStatus;
    }

    virtual size_t frameCount() OVERRIDE { return 1; }
    virtual int repetitionCount() const OVERRIDE { return cAnimationNone; }
    virtual float frameDuration() const OVERRIDE { return 0; }

protected:
    PassOwnPtr<ScaledImageFragment> createCompleteImage(const SkISize& size)
    {
        SkBitmap bitmap;
        bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
        bitmap.allocPixels();
        return ScaledImageFragment::createComplete(size, 0, bitmap);
    }

    void addNewData()
    {
        m_data->append("g", 1);
        m_generator->setData(m_data, false);
    }

    void setFrameStatus(ImageFrame::Status status)  { m_status = m_nextFrameStatus = status; }
    void setNextFrameStatus(ImageFrame::Status status)  { m_nextFrameStatus = status; }

    RefPtr<SharedBuffer> m_data;
    RefPtr<ImageFrameGenerator> m_generator;
    int m_decodersDestroyed;
    int m_frameBufferRequestCount;
    ImageFrame::Status m_status;
    ImageFrame::Status m_nextFrameStatus;
};

TEST_F(ImageFrameGeneratorTest, cacheHit)
{
    const ScaledImageFragment* fullImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(fullSize()));
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_EQ(fullImage, tempImage);
    EXPECT_EQ(fullSize(), tempImage->scaledSize());
    EXPECT_TRUE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithScale)
{
    const ScaledImageFragment* fullImage = ImageDecodingStore::instance()->insertAndLockCache(
        m_generator.get(), createCompleteImage(fullSize()));
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    // Cache miss because of scaled size not found.
    const ScaledImageFragment* scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_NE(fullImage, scaledImage);
    EXPECT_EQ(scaledSize(), scaledImage->scaledSize());
    EXPECT_TRUE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);

    // Cache hit.
    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(scaledImage, tempImage);
    EXPECT_EQ(scaledSize(), tempImage->scaledSize());
    EXPECT_TRUE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(0, m_frameBufferRequestCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithDecodeAndScale)
{
    setFrameStatus(ImageFrame::FrameComplete);

    // Cache miss.
    const ScaledImageFragment* scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(scaledSize(), scaledImage->scaledSize());
    EXPECT_FALSE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);
    EXPECT_EQ(1, m_decodersDestroyed);

    // Cache hit.
    const ScaledImageFragment* fullImage = m_generator->decodeAndScale(fullSize());
    EXPECT_NE(scaledImage, fullImage);
    EXPECT_EQ(fullSize(), fullImage->scaledSize());
    EXPECT_FALSE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);

    // Cache hit.
    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_EQ(scaledImage, tempImage);
    EXPECT_EQ(scaledSize(), tempImage->scaledSize());
    EXPECT_FALSE(m_generator->hasAlpha(0));
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(1, m_frameBufferRequestCount);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecode)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());

    addNewData();
    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, cacheMissWithIncompleteDecodeAndScale)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());

    addNewData();
    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(5u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(4u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesComplete)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());

    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(0u, ImageDecodingStore::instance()->decoderCacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeAndScaleBecomesComplete)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(3u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());

    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();

    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(4u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(4u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(0u, ImageDecodingStore::instance()->decoderCacheEntries());

    tempImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_TRUE(tempImage->isComplete());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    EXPECT_EQ(2, m_frameBufferRequestCount);
}

static void decodeThreadMain(void* arg)
{
    ImageFrameGenerator* generator = reinterpret_cast<ImageFrameGenerator*>(arg);
    const ScaledImageFragment* tempImage = generator->decodeAndScale(fullSize());
    ImageDecodingStore::instance()->unlockCache(generator, tempImage);
}

TEST_F(ImageFrameGeneratorTest, incompleteDecodeBecomesCompleteMultiThreaded)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);
    EXPECT_EQ(0, m_decodersDestroyed);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());

    // Frame can now be decoded completely.
    setFrameStatus(ImageFrame::FrameComplete);
    addNewData();
    ThreadIdentifier threadID = createThread(&decodeThreadMain, m_generator.get(), "DecodeThread");
    waitForThreadCompletion(threadID);

    EXPECT_EQ(2, m_frameBufferRequestCount);
    EXPECT_EQ(1, m_decodersDestroyed);
    EXPECT_EQ(2u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(2u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(0u, ImageDecodingStore::instance()->decoderCacheEntries());

    tempImage = m_generator->decodeAndScale(fullSize());
    EXPECT_TRUE(tempImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
}

TEST_F(ImageFrameGeneratorTest, concurrentIncompleteDecodeAndScale)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* fullImage = m_generator->decodeAndScale(fullSize());
    const ScaledImageFragment* scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_FALSE(fullImage->isComplete());
    EXPECT_FALSE(scaledImage->isComplete());
    EXPECT_EQ(2, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), fullImage);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);
    EXPECT_EQ(4u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(3u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(1u, ImageDecodingStore::instance()->decoderCacheEntries());
    EXPECT_EQ(0, m_decodersDestroyed);

    addNewData();
    setFrameStatus(ImageFrame::FrameComplete);
    scaledImage = m_generator->decodeAndScale(scaledSize());
    EXPECT_TRUE(scaledImage->isComplete());
    EXPECT_EQ(3, m_frameBufferRequestCount);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), scaledImage);
    EXPECT_EQ(5u, ImageDecodingStore::instance()->cacheEntries());
    EXPECT_EQ(5u, ImageDecodingStore::instance()->imageCacheEntries());
    EXPECT_EQ(0u, ImageDecodingStore::instance()->decoderCacheEntries());
    EXPECT_EQ(1, m_decodersDestroyed);
}

TEST_F(ImageFrameGeneratorTest, incompleteBitmapCopied)
{
    setFrameStatus(ImageFrame::FramePartial);

    const ScaledImageFragment* tempImage= m_generator->decodeAndScale(fullSize());
    EXPECT_FALSE(tempImage->isComplete());
    EXPECT_EQ(1, m_frameBufferRequestCount);

    ImageDecoder* tempDecoder = 0;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), fullSize(), &tempDecoder));
    ASSERT_TRUE(tempDecoder);
    EXPECT_NE(tempDecoder->frameBufferAtIndex(0)->getSkBitmap().getPixels(), tempImage->bitmap().getPixels());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);
    ImageDecodingStore::instance()->unlockDecoder(m_generator.get(), tempDecoder);
}

TEST_F(ImageFrameGeneratorTest, resumeDecodeEmptyFrameTurnsComplete)
{
    m_generator = ImageFrameGenerator::create(fullSize(), m_data, false, true);
    m_generator->setImageDecoderFactoryForTesting(MockImageDecoderFactory::create(this, fullSize()));
    setFrameStatus(ImageFrame::FrameComplete);

    const ScaledImageFragment* tempImage = m_generator->decodeAndScale(fullSize(), 0);
    EXPECT_TRUE(tempImage->isComplete());
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), tempImage);

    setFrameStatus(ImageFrame::FrameEmpty);
    setNextFrameStatus(ImageFrame::FrameComplete);
    EXPECT_FALSE(m_generator->decodeAndScale(fullSize(), 1));
}

TEST_F(ImageFrameGeneratorTest, frameHasAlpha)
{
    setFrameStatus(ImageFrame::FramePartial);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), m_generator->decodeAndScale(fullSize(), 1));
    EXPECT_TRUE(m_generator->hasAlpha(1));

    ImageDecoder* tempDecoder = 0;
    EXPECT_TRUE(ImageDecodingStore::instance()->lockDecoder(m_generator.get(), fullSize(), &tempDecoder));
    ASSERT_TRUE(tempDecoder);
    static_cast<MockImageDecoder*>(tempDecoder)->setFrameHasAlpha(false);
    ImageDecodingStore::instance()->unlockDecoder(m_generator.get(), tempDecoder);

    setFrameStatus(ImageFrame::FrameComplete);
    ImageDecodingStore::instance()->unlockCache(m_generator.get(), m_generator->decodeAndScale(fullSize(), 1));
    EXPECT_FALSE(m_generator->hasAlpha(1));
}

} // namespace
