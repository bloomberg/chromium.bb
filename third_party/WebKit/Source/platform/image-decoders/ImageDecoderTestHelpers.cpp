// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/ImageDecoderTestHelpers.h"

#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"
#include "wtf/StringHasher.h"

namespace blink {

PassRefPtr<SharedBuffer> readFile(const char* fileName)
{
    String filePath = testing::blinkRootDir();
    filePath.append(fileName);
    return testing::readFromFile(filePath);
}

PassRefPtr<SharedBuffer> readFile(const char* dir, const char* fileName)
{
    String filePath = testing::blinkRootDir();
    filePath.append("/");
    filePath.append(dir);
    filePath.append("/");
    filePath.append(fileName);

    return testing::readFromFile(filePath);
}

unsigned hashBitmap(const SkBitmap& bitmap)
{
    return StringHasher::hashMemory(bitmap.getPixels(), bitmap.getSize());
}

static unsigned createDecodingBaseline(DecoderCreator createDecoder, SharedBuffer* data)
{
    OwnPtr<ImageDecoder> decoder = createDecoder();
    decoder->setData(data, true);
    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    return hashBitmap(frame->bitmap());
}

void createDecodingBaseline(DecoderCreator createDecoder, SharedBuffer* data, Vector<unsigned>* baselineHashes)
{
    OwnPtr<ImageDecoder> decoder = createDecoder();
    decoder->setData(data, true);
    size_t frameCount = decoder->frameCount();
    for (size_t i = 0; i < frameCount; ++i) {
        ImageFrame* frame = decoder->frameBufferAtIndex(i);
        baselineHashes->append(hashBitmap(frame->getSkBitmap()));
    }
}

void testByteByByteDecode(DecoderCreator createDecoder, const char* file, size_t expectedFrameCount, int expectedRepetitionCount)
{
    RefPtr<SharedBuffer> data = readFile(file);
    ASSERT_TRUE(data.get());
    ASSERT_TRUE(data->data());

    Vector<unsigned> baselineHashes;
    createDecodingBaseline(createDecoder, data.get(), &baselineHashes);

    OwnPtr<ImageDecoder> decoder = createDecoder();

    size_t frameCount = 0;
    size_t framesDecoded = 0;

    // Pass data to decoder byte by byte.
    RefPtr<SharedBuffer> sourceData[2] = { SharedBuffer::create(), SharedBuffer::create() };
    const char* source = data->data();

    for (size_t length = 1; length <= data->size() && !decoder->failed(); ++length) {
        sourceData[0]->append(source, 1u);
        sourceData[1]->append(source++, 1u);
        // Alternate the buffers to cover the JPEGImageDecoder::onSetData restart code.
        decoder->setData(sourceData[length & 1].get(), length == data->size());

        EXPECT_LE(frameCount, decoder->frameCount());
        frameCount = decoder->frameCount();

        if (!decoder->isSizeAvailable())
            continue;

        ImageFrame* frame = decoder->frameBufferAtIndex(frameCount - 1);
        if (frame && frame->status() == ImageFrame::FrameComplete && framesDecoded < frameCount)
            ++framesDecoded;
    }

    EXPECT_FALSE(decoder->failed());
    EXPECT_EQ(expectedFrameCount, decoder->frameCount());
    EXPECT_EQ(expectedFrameCount, framesDecoded);
    EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount());

    ASSERT_EQ(expectedFrameCount, baselineHashes.size());
    for (size_t i = 0; i < decoder->frameCount(); i++) {
        ImageFrame* frame = decoder->frameBufferAtIndex(i);
        EXPECT_EQ(baselineHashes[i], hashBitmap(frame->getSkBitmap()));
    }
}

// This test verifies that calling SharedBuffer::mergeSegmentsIntoBuffer() does
// not break decoding at a critical point: in between a call to decode the size
// (when the decoder stops while it may still have input data to read) and a
// call to do a full decode.
void testMergeBuffer(DecoderCreator createDecoder, const char* file)
{
    RefPtr<SharedBuffer> data = readFile(file);
    ASSERT_TRUE(data);

    const unsigned hash = createDecodingBaseline(createDecoder, data.get());

    // In order to do any verification, this test needs to move the data owned
    // by the SharedBuffer. A way to guarantee that is to create a new one, and
    // then append a string of characters greater than kSegmentSize. This
    // results in writing the data into a segment, skipping the internal
    // contiguous buffer.
    RefPtr<SharedBuffer> segmentedData = SharedBuffer::create();
    segmentedData->append(data->data(), data->size());

    OwnPtr<ImageDecoder> decoder = createDecoder();
    decoder->setData(segmentedData.get(), true);

    ASSERT_TRUE(decoder->isSizeAvailable());

    // This will call SharedBuffer::mergeSegmentsIntoBuffer, copying all
    // segments into the contiguous buffer. If the ImageDecoder was pointing to
    // data in a segment, its pointer would no longer be valid.
    segmentedData->data();

    ImageFrame* frame = decoder->frameBufferAtIndex(0);
    ASSERT_FALSE(decoder->failed());
    EXPECT_EQ(frame->status(), ImageFrame::FrameComplete);
    EXPECT_EQ(hashBitmap(frame->bitmap()), hash);
}

} // namespace blink
