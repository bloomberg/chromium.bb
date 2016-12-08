// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/png/PNGImageDecoder.h"

#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> createDecoder(
    ImageDecoder::AlphaOption alphaOption) {
  return WTF::wrapUnique(new PNGImageDecoder(
      alphaOption, ColorBehavior::transformToTargetForTesting(),
      ImageDecoder::noDecodedImageByteLimit));
}

std::unique_ptr<ImageDecoder> createDecoder() {
  return createDecoder(ImageDecoder::AlphaNotPremultiplied);
}

std::unique_ptr<ImageDecoder> createDecoderWithPngData(const char* pngFile) {
  auto decoder = createDecoder();
  auto data = readFile(pngFile);
  EXPECT_FALSE(data->isEmpty());
  decoder->setData(data.get(), true);
  return decoder;
}

void testSize(const char* pngFile, IntSize expectedSize) {
  auto decoder = createDecoderWithPngData(pngFile);
  EXPECT_TRUE(decoder->isSizeAvailable());
  EXPECT_EQ(expectedSize, decoder->size());
}

void testRepetitionCount(const char* pngFile, int expectedRepetitionCount) {
  auto decoder = createDecoderWithPngData(pngFile);
  // Decoding the frame count sets the repetition count as well.
  decoder->frameCount();
  EXPECT_FALSE(decoder->failed());
  EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount());
}

// Verify that the decoder can successfully decode the first frame when
// initially only half of the frame data is received, resulting in a partially
// decoded image, and then the rest of the image data is received. Verify that
// the bitmap hashes of the two stages are different. Also verify that the final
// bitmap hash is equivalent to the hash when all data is provided at once.
//
// This verifies that decoder correctly keeps track of where it stopped
// decoding when the image was not yet fully received.
void testProgressiveDecodingContinuesAfterFullData(const char* pngFile,
                                                   size_t offsetMidFirstFrame) {
  auto fullData = readFile(pngFile);
  ASSERT_FALSE(fullData->isEmpty());

  auto decoderUpfront = createDecoder();
  decoderUpfront->setData(fullData.get(), true);
  EXPECT_GE(1u, decoderUpfront->frameCount());
  const ImageFrame* const frameUpfront = decoderUpfront->frameBufferAtIndex(0);
  ASSERT_EQ(ImageFrame::FrameComplete, frameUpfront->getStatus());
  const unsigned hashUpfront = hashBitmap(frameUpfront->bitmap());

  auto decoder = createDecoder();
  RefPtr<SharedBuffer> partialData =
      SharedBuffer::create(fullData->data(), offsetMidFirstFrame);
  decoder->setData(partialData, false);

  EXPECT_EQ(1u, decoder->frameCount());
  const ImageFrame* frame = decoder->frameBufferAtIndex(0);
  EXPECT_EQ(frame->getStatus(), ImageFrame::FramePartial);
  const unsigned hashPartial = hashBitmap(frame->bitmap());

  decoder->setData(fullData.get(), true);
  frame = decoder->frameBufferAtIndex(0);
  EXPECT_EQ(frame->getStatus(), ImageFrame::FrameComplete);
  const unsigned hashFull = hashBitmap(frame->bitmap());

  EXPECT_FALSE(decoder->failed());
  EXPECT_NE(hashFull, hashPartial);
  EXPECT_EQ(hashFull, hashUpfront);
}

// Modify the frame data bytes for frame |frameIndex| so that decoding fails.
// Parsing should work fine, and is checked with |expectedFrameCountBefore|. If
// the failure should invalidate the decoder, |expectFailure| should be set to
// true. If not, |expectedFrameCountAfter| should indicate the new frame count
// after the failure.
void testFailureDuringDecode(const char* file,
                             size_t idatOffset,
                             size_t frameIndex,
                             bool expectFailure,
                             size_t expectedFrameCountBefore,
                             size_t expectedFrameCountAfter = 0u) {
  RefPtr<SharedBuffer> fullData = readFile(file);
  ASSERT_FALSE(fullData->isEmpty());

  // This is the offset where the frame data chunk frame |frameIndex| starts.
  RefPtr<SharedBuffer> data =
      SharedBuffer::create(fullData->data(), idatOffset + 8u);
  // Repeat the first 8 bytes of the frame data. This should result in a
  // successful parse, since frame data is not analyzed in that step, but
  // should give an error during decoding.
  data->append(fullData->data() + idatOffset, 8u);
  data->append(fullData->data() + idatOffset + 16u,
               fullData->size() - idatOffset - 16u);

  auto decoder = createDecoder();
  decoder->setData(data.get(), true);

  EXPECT_EQ(expectedFrameCountBefore, decoder->frameCount());

  const ImageFrame* const frame = decoder->frameBufferAtIndex(frameIndex);
  EXPECT_EQ(expectFailure, decoder->failed());
  if (!expectFailure) {
    EXPECT_EQ(expectedFrameCountAfter, decoder->frameCount());
    EXPECT_EQ(ImageFrame::FrameEmpty, frame->getStatus());
  }
}

}  // Anonymous namespace

// Static PNG tests

TEST(StaticPNGTests, repetitionCountTest) {
  testRepetitionCount("/LayoutTests/images/resources/png-simple.png",
                      cAnimationNone);
}

TEST(StaticPNGTests, sizeTest) {
  testSize("/LayoutTests/images/resources/png-simple.png", IntSize(111, 29));
}

TEST(StaticPNGTests, MetaDataTest) {
  const size_t expectedFrameCount = 1;
  const size_t expectedDuration = 0;
  auto decoder =
      createDecoderWithPngData("/LayoutTests/images/resources/png-simple.png");
  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
  EXPECT_EQ(expectedDuration, decoder->frameDurationAtIndex(0));
}

TEST(StaticPNGTests, ProgressiveDecoding) {
  testProgressiveDecoding(&createDecoder,
                          "/LayoutTests/images/resources/png-simple.png", 11u);
}

TEST(StaticPNGTests, ProgressiveDecodingContinuesAfterFullData) {
  testProgressiveDecodingContinuesAfterFullData(
      "/LayoutTests/images/resources/png-simple.png", 1000u);
}

TEST(StaticPNGTests, FailureDuringDecodingInvalidatesDecoder) {
  testFailureDuringDecode(
      "/LayoutTests/images/resources/png-simple.png",
      85u,   // idat offset for frame index 0
      0u,    // try to decode frame index 0
      true,  // expect the decoder to be invalidated after the failure
      1u);   // expected frame count before failure
}

// For static images, frameIsCompleteAtIndex(0) should return true if and only
// if the frame is successfully decoded, not when it is fully received.
TEST(StaticPNGTests, VerifyFrameCompleteBehavior) {
  auto decoder =
      createDecoderWithPngData("/LayoutTests/images/resources/png-simple.png");
  EXPECT_EQ(1u, decoder->frameCount());
  EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));
  EXPECT_EQ(ImageFrame::FrameComplete,
            decoder->frameBufferAtIndex(0)->getStatus());
  EXPECT_TRUE(decoder->frameIsCompleteAtIndex(0));
}

};  // namespace blink
