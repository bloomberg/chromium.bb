/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"

#include "core/platform/image-decoders/ImageDecoder.h"

#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

using namespace WebCore;

class TestImageDecoder : public ImageDecoder {
public:
    TestImageDecoder()
        : ImageDecoder(ImageSource::AlphaNotPremultiplied, ImageSource::GammaAndColorProfileApplied)
    {
    }

    virtual String filenameExtension() const OVERRIDE { return ""; }
    virtual ImageFrame* frameBufferAtIndex(size_t) OVERRIDE { return 0; }

    Vector<ImageFrame, 1>& frameBufferCache()
    {
        return m_frameBufferCache;
    }

    void resetRequiredPreviousFrames()
    {
        for (size_t i = 0; i < m_frameBufferCache.size(); ++i)
            m_frameBufferCache[i].setRequiredPreviousFrameIndex(findRequiredPreviousFrame(i));
    }

    void initFrames(size_t numFrames, unsigned width = 100, unsigned height = 100)
    {
        setSize(width, height);
        m_frameBufferCache.resize(numFrames);
        for (size_t i = 0; i < numFrames; ++i)
            m_frameBufferCache[i].setOriginalFrameRect(IntRect(0, 0, width, height));
    }
};

TEST(ImageDecoderTest, requiredPreviousFrameIndex)
{
    OwnPtr<TestImageDecoder> decoder(adoptPtr(new TestImageDecoder()));
    decoder->initFrames(6);
    Vector<ImageFrame, 1>& decoderFrameBufferCache = decoder->frameBufferCache();

    decoderFrameBufferCache[1].setDisposalMethod(ImageFrame::DisposeKeep);
    decoderFrameBufferCache[2].setDisposalMethod(ImageFrame::DisposeOverwritePrevious);
    decoderFrameBufferCache[3].setDisposalMethod(ImageFrame::DisposeOverwritePrevious);
    decoderFrameBufferCache[4].setDisposalMethod(ImageFrame::DisposeKeep);

    decoder->resetRequiredPreviousFrames();

    // The first frame doesn't require any previous frame.
    EXPECT_EQ(notFound, decoderFrameBufferCache[0].requiredPreviousFrameIndex());
    // The previous DisposeNotSpecified frame is required.
    EXPECT_EQ(0u, decoderFrameBufferCache[1].requiredPreviousFrameIndex());
    // DisposeKeep is treated as DisposeNotSpecified.
    EXPECT_EQ(1u, decoderFrameBufferCache[2].requiredPreviousFrameIndex());
    // Previous DisposeOverwritePrevious frames are skipped.
    EXPECT_EQ(1u, decoderFrameBufferCache[3].requiredPreviousFrameIndex());
    EXPECT_EQ(1u, decoderFrameBufferCache[4].requiredPreviousFrameIndex());
    EXPECT_EQ(4u, decoderFrameBufferCache[5].requiredPreviousFrameIndex());
}

TEST(ImageDecoderTest, requiredPreviousFrameIndexDisposeOverwriteBgcolor)
{
    OwnPtr<TestImageDecoder> decoder(adoptPtr(new TestImageDecoder()));
    decoder->initFrames(3);
    Vector<ImageFrame, 1>& decoderFrameBufferCache = decoder->frameBufferCache();

    // Fully covering DisposeOverwriteBgcolor previous frame resets the starting state.
    decoderFrameBufferCache[1].setDisposalMethod(ImageFrame::DisposeOverwriteBgcolor);
    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(notFound, decoderFrameBufferCache[2].requiredPreviousFrameIndex());

    // Partially covering DisposeOverwriteBgcolor previous frame is required by this frame.
    decoderFrameBufferCache[1].setOriginalFrameRect(IntRect(50, 50, 50, 50));
    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(1u, decoderFrameBufferCache[2].requiredPreviousFrameIndex());
}

TEST(ImageDecoderTest, requiredPreviousFrameIndexForFrame1)
{
    OwnPtr<TestImageDecoder> decoder(adoptPtr(new TestImageDecoder()));
    decoder->initFrames(2);
    Vector<ImageFrame, 1>& decoderFrameBufferCache = decoder->frameBufferCache();

    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(0u, decoderFrameBufferCache[1].requiredPreviousFrameIndex());

    // The first frame with DisposeOverwritePrevious or DisposeOverwriteBgcolor
    // resets the starting state.
    decoderFrameBufferCache[0].setDisposalMethod(ImageFrame::DisposeOverwritePrevious);
    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(notFound, decoderFrameBufferCache[1].requiredPreviousFrameIndex());
    decoderFrameBufferCache[0].setDisposalMethod(ImageFrame::DisposeOverwriteBgcolor);
    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(notFound, decoderFrameBufferCache[1].requiredPreviousFrameIndex());

    // ... even if it partially covers.
    decoderFrameBufferCache[0].setOriginalFrameRect(IntRect(50, 50, 50, 50));

    decoderFrameBufferCache[0].setDisposalMethod(ImageFrame::DisposeOverwritePrevious);
    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(notFound, decoderFrameBufferCache[1].requiredPreviousFrameIndex());
    decoderFrameBufferCache[0].setDisposalMethod(ImageFrame::DisposeOverwriteBgcolor);
    decoder->resetRequiredPreviousFrames();
    EXPECT_EQ(notFound, decoderFrameBufferCache[1].requiredPreviousFrameIndex());
}

TEST(ImageDecoderTest, clearCacheExceptFrameDoNothing)
{
    OwnPtr<TestImageDecoder> decoder(adoptPtr(new TestImageDecoder()));
    decoder->clearCacheExceptFrame(0);

    // This should not crash.
    decoder->initFrames(20);
    decoder->clearCacheExceptFrame(notFound);
}

TEST(ImageDecoderTest, clearCacheExceptFrameAll)
{
    const size_t numFrames = 10;
    OwnPtr<TestImageDecoder> decoder(adoptPtr(new TestImageDecoder()));
    decoder->initFrames(numFrames);
    Vector<ImageFrame, 1>& decoderFrameBufferCache = decoder->frameBufferCache();
    for (size_t i = 0; i < numFrames; ++i)
        decoderFrameBufferCache[i].setStatus(i % 2 ? ImageFrame::FramePartial : ImageFrame::FrameComplete);

    decoder->clearCacheExceptFrame(notFound);

    for (size_t i = 0; i < numFrames; ++i) {
        SCOPED_TRACE(testing::Message() << i);
        EXPECT_EQ(ImageFrame::FrameEmpty, decoderFrameBufferCache[i].status());
    }
}

TEST(ImageDecoderTest, clearCacheExceptFramePreverveClearExceptFrame)
{
    const size_t numFrames = 10;
    OwnPtr<TestImageDecoder> decoder(adoptPtr(new TestImageDecoder()));
    decoder->initFrames(numFrames);
    Vector<ImageFrame, 1>& decoderFrameBufferCache = decoder->frameBufferCache();
    for (size_t i = 0; i < numFrames; ++i)
        decoderFrameBufferCache[i].setStatus(ImageFrame::FrameComplete);

    decoder->resetRequiredPreviousFrames();
    decoder->clearCacheExceptFrame(5);
    for (size_t i = 0; i < numFrames; ++i) {
        SCOPED_TRACE(testing::Message() << i);
        if (i == 5)
            EXPECT_EQ(ImageFrame::FrameComplete, decoderFrameBufferCache[i].status());
        else
            EXPECT_EQ(ImageFrame::FrameEmpty, decoderFrameBufferCache[i].status());
    }
}
