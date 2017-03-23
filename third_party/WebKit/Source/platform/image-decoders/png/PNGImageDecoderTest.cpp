// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/image-decoders/png/PNGImageDecoder.h"

#include <memory>
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "png.h"
#include "testing/gtest/include/gtest/gtest.h"

// /LayoutTests/images/resources/png-animated-idat-part-of-animation.png
// is modified in multiple tests to simulate erroneous PNGs. As a reference,
// the table below shows how the file is structured.
//
// Offset | 8     33    95    133   172   210   241   279   314   352   422
// -------------------------------------------------------------------------
// Chunk  | IHDR  acTL  fcTL  IDAT  fcTL  fdAT  fcTL  fdAT  fcTL  fdAT  IEND
//
// In between the acTL and fcTL there are two other chunks, PLTE and tRNS, but
// those are not specifically used in this test suite. The same holds for a
// tEXT chunk in between the last fdAT and IEND.
//
// In the current behavior of PNG image decoders, the 4 frames are detected when
// respectively 141, 249, 322 and 430 bytes are received. The first frame should
// be detected when the IDAT has been received, and non-first frames when the
// next fcTL or IEND chunk has been received. Note that all offsets are +8,
// because a chunk is identified by byte 4-7.

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

// Test whether querying for the size of the image works if we present the
// data byte by byte.
void testSizeByteByByte(const char* pngFile,
                        size_t bytesNeededToDecodeSize,
                        IntSize expectedSize) {
  auto decoder = createDecoder();
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());
  ASSERT_LT(bytesNeededToDecodeSize, data->size());

  const char* source = data->data();
  RefPtr<SharedBuffer> partialData = SharedBuffer::create();
  for (size_t length = 1; length <= bytesNeededToDecodeSize; length++) {
    partialData->append(source++, 1u);
    decoder->setData(partialData.get(), false);

    if (length < bytesNeededToDecodeSize) {
      EXPECT_FALSE(decoder->isSizeAvailable());
      EXPECT_TRUE(decoder->size().isEmpty());
      EXPECT_FALSE(decoder->failed());
    } else {
      EXPECT_TRUE(decoder->isSizeAvailable());
      EXPECT_EQ(expectedSize, decoder->size());
    }
  }
  EXPECT_FALSE(decoder->failed());
}

void writeUint32(uint32_t val, png_byte* data) {
  data[0] = val >> 24;
  data[1] = val >> 16;
  data[2] = val >> 8;
  data[3] = val;
}

void testRepetitionCount(const char* pngFile, int expectedRepetitionCount) {
  auto decoder = createDecoderWithPngData(pngFile);
  // Decoding the frame count sets the number of repetitions as well.
  decoder->frameCount();
  EXPECT_FALSE(decoder->failed());
  EXPECT_EQ(expectedRepetitionCount, decoder->repetitionCount());
}

struct PublicFrameInfo {
  size_t duration;
  IntRect frameRect;
  ImageFrame::AlphaBlendSource alphaBlend;
  ImageFrame::DisposalMethod disposalMethod;
};

// This is the frame data for the following PNG image:
// /LayoutTests/images/resources/png-animated-idat-part-of-animation.png
static PublicFrameInfo pngAnimatedFrameInfo[] = {
    {500,
     {IntPoint(0, 0), IntSize(5, 5)},
     ImageFrame::BlendAtopBgcolor,
     ImageFrame::DisposeKeep},
    {900,
     {IntPoint(1, 1), IntSize(3, 1)},
     ImageFrame::BlendAtopBgcolor,
     ImageFrame::DisposeOverwriteBgcolor},
    {2000,
     {IntPoint(1, 2), IntSize(3, 2)},
     ImageFrame::BlendAtopPreviousFrame,
     ImageFrame::DisposeKeep},
    {1500,
     {IntPoint(1, 2), IntSize(3, 1)},
     ImageFrame::BlendAtopBgcolor,
     ImageFrame::DisposeKeep},
};

void compareFrameWithExpectation(const PublicFrameInfo& expected,
                                 ImageDecoder* decoder,
                                 size_t index) {
  EXPECT_EQ(expected.duration, decoder->frameDurationAtIndex(index));

  const auto* frame = decoder->frameBufferAtIndex(index);
  ASSERT_TRUE(frame);

  EXPECT_EQ(expected.duration, frame->duration());
  EXPECT_EQ(expected.frameRect, frame->originalFrameRect());
  EXPECT_EQ(expected.disposalMethod, frame->getDisposalMethod());
  EXPECT_EQ(expected.alphaBlend, frame->getAlphaBlendSource());
}

// This function removes |length| bytes at |offset|, and then calls frameCount.
// It assumes the missing bytes should result in a failed decode because the
// parser jumps |length| bytes too far in the next chunk.
void testMissingDataBreaksDecoding(const char* pngFile,
                                   size_t offset,
                                   size_t length) {
  auto decoder = createDecoder();
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());

  RefPtr<SharedBuffer> invalidData = SharedBuffer::create(data->data(), offset);
  invalidData->append(data->data() + offset + length,
                      data->size() - offset - length);
  ASSERT_EQ(data->size() - length, invalidData->size());

  decoder->setData(invalidData, true);
  decoder->frameCount();
  EXPECT_TRUE(decoder->failed());
}

// Verify that a decoder with a parse error converts to a static image.
static void expectStatic(ImageDecoder* decoder) {
  EXPECT_EQ(1u, decoder->frameCount());
  EXPECT_FALSE(decoder->failed());

  ImageFrame* frame = decoder->frameBufferAtIndex(0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
  EXPECT_FALSE(decoder->failed());
  EXPECT_EQ(cAnimationNone, decoder->repetitionCount());
}

// Decode up to the indicated fcTL offset and then provide an fcTL with the
// wrong chunk size (20 instead of 26).
void testInvalidFctlSize(const char* pngFile,
                         size_t offsetFctl,
                         size_t expectedFrameCount,
                         bool shouldFail) {
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());

  auto decoder = createDecoder();
  RefPtr<SharedBuffer> invalidData =
      SharedBuffer::create(data->data(), offsetFctl);

  // Test if this gives the correct frame count, before the fcTL is parsed.
  decoder->setData(invalidData, false);
  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
  ASSERT_FALSE(decoder->failed());

  // Append the wrong size to the data stream
  png_byte sizeChunk[4];
  writeUint32(20, sizeChunk);
  invalidData->append(reinterpret_cast<char*>(sizeChunk), 4u);

  // Skip the size in the original data, but provide a truncated fcTL,
  // which is 4B of tag, 20B of data and 4B of CRC, totalling 28B.
  invalidData->append(data->data() + offsetFctl + 4, 28u);
  // Append the rest of the data
  const size_t offsetPostFctl = offsetFctl + 38;
  invalidData->append(data->data() + offsetPostFctl,
                      data->size() - offsetPostFctl);

  decoder->setData(invalidData, false);
  if (shouldFail) {
    EXPECT_EQ(expectedFrameCount, decoder->frameCount());
    EXPECT_EQ(true, decoder->failed());
  } else {
    expectStatic(decoder.get());
  }
}

// Verify that the decoder can successfully decode the first frame when
// initially only half of the frame data is received, resulting in a partially
// decoded image, and then the rest of the image data is received. Verify that
// the bitmap hashes of the two stages are different. Also verify that the final
// bitmap hash is equivalent to the hash when all data is provided at once.
//
// This verifies that the decoder correctly keeps track of where it stopped
// decoding when the image was not yet fully received.
void testProgressiveDecodingContinuesAfterFullData(const char* pngFile,
                                                   size_t offsetMidFirstFrame) {
  auto fullData = readFile(pngFile);
  ASSERT_FALSE(fullData->isEmpty());

  auto decoderUpfront = createDecoder();
  decoderUpfront->setData(fullData.get(), true);
  EXPECT_GE(decoderUpfront->frameCount(), 1u);
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
// Parsing should work fine, and is checked with |expectedFrameCount|.
void testFailureDuringDecode(const char* file,
                             size_t idatOffset,
                             size_t frameIndex,
                             size_t expectedFrameCount) {
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

  EXPECT_EQ(expectedFrameCount, decoder->frameCount());

  decoder->frameBufferAtIndex(frameIndex);
  ASSERT_EQ(true, decoder->failed());

  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
}

}  // Anonymous namespace

// Animated PNG Tests

TEST(AnimatedPNGTests, sizeTest) {
  testSize(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      IntSize(5, 5));
  testSize(
      "/LayoutTests/images/resources/"
      "png-animated-idat-not-part-of-animation.png",
      IntSize(227, 35));
}

TEST(AnimatedPNGTests, repetitionCountTest) {
  testRepetitionCount(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      6u);
  // This is an "animated" image with only one frame, that is, the IDAT is
  // ignored and there is one fdAT frame. so it should be considered
  // non-animated.
  testRepetitionCount(
      "/LayoutTests/images/resources/"
      "png-animated-idat-not-part-of-animation.png",
      cAnimationNone);
}

// Test if the decoded metdata corresponds to the defined expectations
TEST(AnimatedPNGTests, MetaDataTest) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  constexpr size_t expectedFrameCount = 4;

  auto decoder = createDecoderWithPngData(pngFile);
  ASSERT_EQ(expectedFrameCount, decoder->frameCount());
  for (size_t i = 0; i < expectedFrameCount; i++) {
    compareFrameWithExpectation(pngAnimatedFrameInfo[i], decoder.get(), i);
  }
}

TEST(AnimatedPNGTests, EmptyFrame) {
  const char* pngFile = "/LayoutTests/images/resources/empty-frame.png";
  auto decoder = createDecoderWithPngData(pngFile);
  // Frame 0 is empty. Ensure that decoding frame 1 (which depends on frame 0)
  // fails (rather than crashing).
  EXPECT_EQ(2u, decoder->frameCount());
  EXPECT_FALSE(decoder->failed());

  ImageFrame* frame = decoder->frameBufferAtIndex(1);
  EXPECT_TRUE(decoder->failed());
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(ImageFrame::FrameEmpty, frame->getStatus());
}

TEST(AnimatedPNGTests, ByteByByteSizeAvailable) {
  testSizeByteByByte(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      141u, IntSize(5, 5));
  testSizeByteByByte(
      "/LayoutTests/images/resources/"
      "png-animated-idat-not-part-of-animation.png",
      79u, IntSize(227, 35));
}

TEST(AnimatedPNGTests, ByteByByteMetaData) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  constexpr size_t expectedFrameCount = 4;

  // These are the byte offsets where each frame should have been parsed.
  // It boils down to the offset of the first fcTL / IEND after the last
  // frame data chunk, plus 8 bytes for recognition. The exception on this is
  // the first frame, which is reported when its first framedata is seen.
  size_t frameOffsets[expectedFrameCount] = {141, 249, 322, 430};

  auto decoder = createDecoder();
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());
  size_t framesParsed = 0;

  const char* source = data->data();
  RefPtr<SharedBuffer> partialData = SharedBuffer::create();
  for (size_t length = 1; length <= frameOffsets[expectedFrameCount - 1];
       length++) {
    partialData->append(source++, 1u);
    decoder->setData(partialData.get(), false);
    EXPECT_FALSE(decoder->failed());
    if (length < frameOffsets[framesParsed]) {
      EXPECT_EQ(framesParsed, decoder->frameCount());
    } else {
      ASSERT_EQ(framesParsed + 1, decoder->frameCount());
      compareFrameWithExpectation(pngAnimatedFrameInfo[framesParsed],
                                  decoder.get(), framesParsed);
      framesParsed++;
    }
  }
  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
  EXPECT_FALSE(decoder->failed());
}

TEST(AnimatedPNGTests, TestRandomFrameDecode) {
  testRandomFrameDecode(&createDecoder,
                        "/LayoutTests/images/resources/"
                        "png-animated-idat-part-of-animation.png",
                        2u);
}

TEST(AnimatedPNGTests, TestDecodeAfterReallocation) {
  testDecodeAfterReallocatingData(&createDecoder,
                                  "/LayoutTests/images/resources/"
                                  "png-animated-idat-part-of-animation.png");
}

TEST(AnimatedPNGTests, ProgressiveDecode) {
  testProgressiveDecoding(&createDecoder,
                          "/LayoutTests/images/resources/"
                          "png-animated-idat-part-of-animation.png",
                          13u);
}

TEST(AnimatedPNGTests, ParseAndDecodeByteByByte) {
  testByteByByteDecode(&createDecoder,
                       "/LayoutTests/images/resources/"
                       "png-animated-idat-part-of-animation.png",
                       4u, 6u);
}

TEST(AnimatedPNGTests, FailureDuringParsing) {
  // Test the first fcTL in the stream. Because no frame data has been set at
  // this point, the expected frame count is zero. 95 bytes is just before the
  // first fcTL chunk, at which the first frame is detected. This is before the
  // IDAT, so it should be treated as a static image.
  testInvalidFctlSize(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      95u, 0u, false);

  // Test for the third fcTL in the stream. This should see 1 frame before the
  // fcTL, and then fail when parsing it.
  testInvalidFctlSize(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      241u, 1u, true);
}

TEST(AnimatedPNGTests, ActlErrors) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());

  const size_t offsetActl = 33u;
  const size_t acTLSize = 20u;
  {
    // Remove the acTL chunk from the stream. This results in a static image.
    RefPtr<SharedBuffer> noActlData =
        SharedBuffer::create(data->data(), offsetActl);
    noActlData->append(data->data() + offsetActl + acTLSize,
                       data->size() - offsetActl - acTLSize);

    auto decoder = createDecoder();
    decoder->setData(noActlData, true);
    EXPECT_EQ(1u, decoder->frameCount());
    EXPECT_FALSE(decoder->failed());
    EXPECT_EQ(cAnimationNone, decoder->repetitionCount());
  }

  // Store the acTL for more tests.
  char acTL[acTLSize];
  memcpy(acTL, data->data() + offsetActl, acTLSize);

  // Insert an extra acTL at a couple of different offsets.
  // Prior to the IDAT, this should result in a static image. After, this
  // should fail.
  const struct {
    size_t offset;
    bool shouldFail;
  } gRecs[] = {{8u, false},
               {offsetActl, false},
               {133u, false},
               {172u, true},
               {422u, true}};
  for (const auto& rec : gRecs) {
    const size_t offset = rec.offset;
    RefPtr<SharedBuffer> extraActlData =
        SharedBuffer::create(data->data(), offset);
    extraActlData->append(acTL, acTLSize);
    extraActlData->append(data->data() + offset, data->size() - offset);
    auto decoder = createDecoder();
    decoder->setData(extraActlData, true);
    EXPECT_EQ(rec.shouldFail ? 0u : 1u, decoder->frameCount());
    EXPECT_EQ(rec.shouldFail, decoder->failed());
  }

  // An acTL after IDAT is ignored.
  pngFile =
      "/LayoutTests/images/resources/"
      "cHRM_color_spin.png";
  {
    auto data2 = readFile(pngFile);
    ASSERT_FALSE(data2->isEmpty());
    const size_t postIDATOffset = 30971u;
    for (size_t times = 0; times < 2; times++) {
      RefPtr<SharedBuffer> extraActlData =
          SharedBuffer::create(data2->data(), postIDATOffset);
      for (size_t i = 0; i < times; i++)
        extraActlData->append(acTL, acTLSize);
      extraActlData->append(data2->data() + postIDATOffset,
                            data2->size() - postIDATOffset);

      auto decoder = createDecoder();
      decoder->setData(extraActlData, true);
      EXPECT_EQ(1u, decoder->frameCount());
      EXPECT_FALSE(decoder->failed());
      EXPECT_EQ(cAnimationNone, decoder->repetitionCount());
      EXPECT_NE(nullptr, decoder->frameBufferAtIndex(0));
      EXPECT_FALSE(decoder->failed());
    }
  }
}

TEST(AnimatedPNGTests, fdatBeforeIdat) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-not-part-of-animation.png";
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());

  // Insert fcTL and fdAT prior to the IDAT
  const size_t idatOffset = 71u;
  RefPtr<SharedBuffer> modifiedData =
      SharedBuffer::create(data->data(), idatOffset);
  // Copy fcTL and fdAT
  const size_t fctlPlusFdatSize = 38u + 1566u;
  modifiedData->append(data->data() + 2519u, fctlPlusFdatSize);
  // Copy IDAT
  modifiedData->append(data->data() + idatOffset, 2448u);
  // Copy the remaining
  modifiedData->append(data->data() + 4123u, 39u + 12u);
  // Data has just been rearranged.
  ASSERT_EQ(data->size(), modifiedData->size());

  {
    // This broken APNG will be treated as a static png.
    auto decoder = createDecoder();
    decoder->setData(modifiedData.get(), true);
    expectStatic(decoder.get());
  }

  {
    // Remove the acTL from the modified image. It now has fdAT before
    // IDAT, but no acTL, so fdAT should be ignored.
    const size_t offsetActl = 33u;
    const size_t acTLSize = 20u;
    RefPtr<SharedBuffer> modifiedData2 =
        SharedBuffer::create(modifiedData->data(), offsetActl);
    modifiedData2->append(modifiedData->data() + offsetActl + acTLSize,
                          modifiedData->size() - offsetActl - acTLSize);
    auto decoder = createDecoder();
    decoder->setData(modifiedData2.get(), true);
    expectStatic(decoder.get());

    // Likewise, if an acTL follows the fdAT, it is ignored.
    const size_t insertionOffset = idatOffset + fctlPlusFdatSize - acTLSize;
    RefPtr<SharedBuffer> modifiedData3 =
        SharedBuffer::create(modifiedData2->data(), insertionOffset);
    modifiedData3->append(data->data() + offsetActl, acTLSize);
    modifiedData3->append(modifiedData2->data() + insertionOffset,
                          modifiedData2->size() - insertionOffset);
    decoder = createDecoder();
    decoder->setData(modifiedData3.get(), true);
    expectStatic(decoder.get());
  }
}

TEST(AnimatedPNGTests, IdatSizeMismatch) {
  // The default image must fill the image
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  auto data = readFile(pngFile);
  ASSERT_FALSE(data->isEmpty());

  const size_t fctlOffset = 95u;
  RefPtr<SharedBuffer> modifiedData =
      SharedBuffer::create(data->data(), fctlOffset);
  const size_t fctlSize = 38u;
  png_byte fctl[fctlSize];
  memcpy(fctl, data->data() + fctlOffset, fctlSize);
  // Set the height to a smaller value, so it does not fill the image.
  writeUint32(3, fctl + 16);
  // Correct the crc
  writeUint32(3210324191, fctl + 34);
  modifiedData->append((const char*)fctl, fctlSize);
  const size_t afterFctl = fctlOffset + fctlSize;
  modifiedData->append(data->data() + afterFctl, data->size() - afterFctl);

  auto decoder = createDecoder();
  decoder->setData(modifiedData.get(), true);
  expectStatic(decoder.get());
}

// Originally, the third frame has an offset of (1,2) and a size of (3,2). By
// changing the offset to (4,4), the frame rect is no longer within the image
// size of 5x5. This results in a failure.
TEST(AnimatedPNGTests, VerifyFrameOutsideImageSizeFails) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  auto data = readFile(pngFile);
  auto decoder = createDecoder();
  ASSERT_FALSE(data->isEmpty());

  const size_t offsetThirdFctl = 241;
  RefPtr<SharedBuffer> modifiedData =
      SharedBuffer::create(data->data(), offsetThirdFctl);
  const size_t fctlSize = 38u;
  png_byte fctl[fctlSize];
  memcpy(fctl, data->data() + offsetThirdFctl, fctlSize);
  // Modify offset and crc.
  writeUint32(4, fctl + 20u);
  writeUint32(4, fctl + 24u);
  writeUint32(3700322018, fctl + 34u);

  modifiedData->append(const_cast<const char*>(reinterpret_cast<char*>(fctl)),
                       fctlSize);
  modifiedData->append(data->data() + offsetThirdFctl + fctlSize,
                       data->size() - offsetThirdFctl - fctlSize);

  decoder->setData(modifiedData, true);

  IntSize expectedSize(5, 5);
  EXPECT_TRUE(decoder->isSizeAvailable());
  EXPECT_EQ(expectedSize, decoder->size());

  const size_t expectedFrameCount = 0;
  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
  EXPECT_TRUE(decoder->failed());
}

TEST(AnimatedPNGTests, ProgressiveDecodingContinuesAfterFullData) {
  // 160u is a randomly chosen offset in the IDAT chunk of the first frame.
  testProgressiveDecodingContinuesAfterFullData(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      160u);
}

TEST(AnimatedPNGTests, RandomDecodeAfterClearFrameBufferCache) {
  testRandomDecodeAfterClearFrameBufferCache(
      &createDecoder,
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      2u);
}

TEST(AnimatedPNGTests, VerifyAlphaBlending) {
  testAlphaBlending(&createDecoder,
                    "/LayoutTests/images/resources/"
                    "png-animated-idat-part-of-animation.png");
}

// This tests if the frame count gets set correctly when parsing frameCount
// fails in one of the parsing queries.
//
// First, enough data is provided such that two frames should be registered.
// The decoder should at this point not be in the failed status.
//
// Then, we provide the rest of the data except for the last IEND chunk, but
// tell the decoder that this is all the data we have.  The frame count should
// be three, since one extra frame should be discovered. The fourth frame
// should *not* be registered since the reader should not be able to determine
// where the frame ends. The decoder should *not* be in the failed state since
// there are three frames which can be shown.
// Attempting to decode the third frame should fail, since the file is
// truncated.
TEST(AnimatedPNGTests, FailureMissingIendChunk) {
  RefPtr<SharedBuffer> fullData = readFile(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png");
  ASSERT_FALSE(fullData->isEmpty());
  auto decoder = createDecoder();

  const size_t offsetTwoFrames = 249;
  const size_t expectedFramesAfter249Bytes = 2;
  RefPtr<SharedBuffer> tempData =
      SharedBuffer::create(fullData->data(), offsetTwoFrames);
  decoder->setData(tempData.get(), false);
  EXPECT_EQ(expectedFramesAfter249Bytes, decoder->frameCount());
  EXPECT_FALSE(decoder->failed());

  // Provide the rest of the data except for the last IEND chunk.
  const size_t expectedFramesAfterAllExcept12Bytes = 3;
  tempData = SharedBuffer::create(fullData->data(), fullData->size() - 12);
  decoder->setData(tempData.get(), true);
  ASSERT_EQ(expectedFramesAfterAllExcept12Bytes, decoder->frameCount());

  for (size_t i = 0; i < expectedFramesAfterAllExcept12Bytes; i++) {
    EXPECT_FALSE(decoder->failed());
    decoder->frameBufferAtIndex(i);
  }

  EXPECT_TRUE(decoder->failed());
}

TEST(AnimatedPNGTests, FailureDuringDecodingInvalidatesDecoder) {
  testFailureDuringDecode(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      291u,  // fdat offset for frame index 2, plus 12 to move past sequence
             // number.
      2u,    // try to decode frame index 2
      4u);   // expected frame count before failure

  testFailureDuringDecode(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png",
      133u,  // idat offset for frame index 0
      0u,    // try to decode frame index 0
      4u);   // expected frame count before failure
}

// Verify that a malformatted PNG, where the IEND appears before any frame data
// (IDAT), invalidates the decoder.
TEST(AnimatedPNGTests, VerifyIENDBeforeIDATInvalidatesDecoder) {
  RefPtr<SharedBuffer> fullData = readFile(
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png");
  ASSERT_FALSE(fullData->isEmpty());
  auto decoder = createDecoder();

  const size_t offsetIDAT = 133;
  RefPtr<SharedBuffer> data =
      SharedBuffer::create(fullData->data(), offsetIDAT);
  data->append(fullData->data() + fullData->size() - 12u, 12u);
  data->append(fullData->data() + offsetIDAT, fullData->size() - offsetIDAT);
  decoder->setData(data.get(), true);

  const size_t expectedFrameCount = 0u;
  EXPECT_EQ(expectedFrameCount, decoder->frameCount());
  EXPECT_TRUE(decoder->failed());
}

// All IDAT chunks must be before all fdAT chunks
TEST(AnimatedPNGTests, MixedDataChunks) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  RefPtr<SharedBuffer> fullData = readFile(pngFile);
  ASSERT_FALSE(fullData->isEmpty());

  // Add an extra fdAT after the first IDAT, skipping fcTL.
  const size_t postIDAT = 172u;
  RefPtr<SharedBuffer> data = SharedBuffer::create(fullData->data(), postIDAT);
  const size_t fcTLSize = 38u;
  const size_t fdATSize = 31u;
  png_byte fdAT[fdATSize];
  memcpy(fdAT, fullData->data() + postIDAT + fcTLSize, fdATSize);
  // Modify the sequence number
  writeUint32(1u, fdAT + 8);
  data->append((const char*)fdAT, fdATSize);
  const size_t IENDOffset = 422u;
  data->append(fullData->data() + IENDOffset, fullData->size() - IENDOffset);
  auto decoder = createDecoder();
  decoder->setData(data.get(), true);
  decoder->frameCount();
  EXPECT_TRUE(decoder->failed());

  // Insert an IDAT after an fdAT.
  const size_t postfdAT = postIDAT + fcTLSize + fdATSize;
  data = SharedBuffer::create(fullData->data(), postfdAT);
  const size_t IDATOffset = 133u;
  data->append(fullData->data() + IDATOffset, postIDAT - IDATOffset);
  // Append the rest.
  data->append(fullData->data() + postIDAT, fullData->size() - postIDAT);
  decoder = createDecoder();
  decoder->setData(data.get(), true);
  decoder->frameCount();
  EXPECT_TRUE(decoder->failed());
}

// Verify that erroneous values for the disposal method and alpha blending
// cause the decoder to fail.
TEST(AnimatedPNGTests, VerifyInvalidDisposalAndBlending) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  RefPtr<SharedBuffer> fullData = readFile(pngFile);
  ASSERT_FALSE(fullData->isEmpty());
  auto decoder = createDecoder();

  // The disposal byte in the frame control chunk is the 24th byte, alpha
  // blending the 25th. |offsetDisposalOp| is 241 bytes to get to the third
  // fctl chunk, 8 bytes to skip the length and tag bytes, and 24 bytes to get
  // to the disposal op.
  //
  // Write invalid values to the disposal and alpha blending byte, correct the
  // crc and append the rest of the buffer.
  const size_t offsetDisposalOp = 241 + 8 + 24;
  RefPtr<SharedBuffer> data =
      SharedBuffer::create(fullData->data(), offsetDisposalOp);
  png_byte disposalAndBlending[6u];
  disposalAndBlending[0] = 7;
  disposalAndBlending[1] = 9;
  writeUint32(2408835439u, disposalAndBlending + 2u);
  data->append(reinterpret_cast<char*>(disposalAndBlending), 6u);
  data->append(fullData->data() + offsetDisposalOp + 6u,
               fullData->size() - offsetDisposalOp - 6u);

  decoder->setData(data.get(), true);
  decoder->frameCount();
  ASSERT_TRUE(decoder->failed());
}

// This test verifies that the following situation does not invalidate the
// decoder:
// - Frame 0 is decoded progressively, but there's not enough data to fully
//   decode it.
// - The rest of the image data is received.
// - Frame X, with X > 0, and X does not depend on frame 0, is decoded.
// - Frame 0 is decoded.
// This is a tricky case since the decoder resets the png struct for each frame,
// and this test verifies that it does not break the decoding of frame 0, even
// though it already started in the first call.
TEST(AnimatedPNGTests, VerifySuccessfulFirstFrameDecodeAfterLaterFrame) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-three-independent-frames.png";
  auto decoder = createDecoder();
  auto fullData = readFile(pngFile);
  ASSERT_FALSE(fullData->isEmpty());

  // 160u is a randomly chosen offset in the IDAT chunk of the first frame.
  const size_t middleFirstFrame = 160u;
  RefPtr<SharedBuffer> data =
      SharedBuffer::create(fullData->data(), middleFirstFrame);
  decoder->setData(data.get(), false);

  ASSERT_EQ(1u, decoder->frameCount());
  ASSERT_EQ(ImageFrame::FramePartial,
            decoder->frameBufferAtIndex(0)->getStatus());

  decoder->setData(fullData.get(), true);
  ASSERT_EQ(3u, decoder->frameCount());
  ASSERT_EQ(ImageFrame::FrameComplete,
            decoder->frameBufferAtIndex(1)->getStatus());
  // The point is that this call does not decode frame 0, which it won't do if
  // it does not have it as its required previous frame.
  ASSERT_EQ(kNotFound,
            decoder->frameBufferAtIndex(1)->requiredPreviousFrameIndex());

  EXPECT_EQ(ImageFrame::FrameComplete,
            decoder->frameBufferAtIndex(0)->getStatus());
  EXPECT_FALSE(decoder->failed());
}

// If the decoder attempts to decode a non-first frame which is subset and
// independent, it needs to discard its png_struct so it can use a modified
// IHDR. Test this by comparing a decode of frame 1 after frame 0 to a decode
// of frame 1 without decoding frame 0.
TEST(AnimatedPNGTests, DecodeFromIndependentFrame) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-part-of-animation.png";
  auto originalData = readFile(pngFile);
  ASSERT_FALSE(originalData->isEmpty());

  // This file almost fits the bill. Modify it to dispose frame 0, making
  // frame 1 independent.
  const size_t kDisposeOffset = 127u;
  auto data = SharedBuffer::create(originalData->data(), kDisposeOffset);
  // 1 Corresponds to APNG_DISPOSE_OP_BACKGROUND
  const char one = '\001';
  data->append(&one, 1u);
  // No need to modify the blend op
  data->append(originalData->data() + kDisposeOffset + 1, 1u);
  // Modify the CRC
  png_byte crc[4];
  writeUint32(2226670956, crc);
  data->append(reinterpret_cast<const char*>(crc), 4u);
  data->append(originalData->data() + data->size(),
               originalData->size() - data->size());
  ASSERT_EQ(originalData->size(), data->size());

  auto decoder = createDecoder();
  decoder->setData(data.get(), true);

  ASSERT_EQ(4u, decoder->frameCount());
  ASSERT_FALSE(decoder->failed());

  auto* frame = decoder->frameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  ASSERT_EQ(ImageFrame::DisposeOverwriteBgcolor, frame->getDisposalMethod());

  frame = decoder->frameBufferAtIndex(1);
  ASSERT_TRUE(frame);
  ASSERT_FALSE(decoder->failed());
  ASSERT_NE(IntRect({}, decoder->size()), frame->originalFrameRect());
  ASSERT_EQ(kNotFound, frame->requiredPreviousFrameIndex());

  const auto hash = hashBitmap(frame->bitmap());

  // Now decode starting from frame 1.
  decoder = createDecoder();
  decoder->setData(data.get(), true);

  frame = decoder->frameBufferAtIndex(1);
  ASSERT_TRUE(frame);
  EXPECT_EQ(hash, hashBitmap(frame->bitmap()));
}

// If the first frame is subset from IHDR (only allowed if the first frame is
// not the default image), the decoder has to destroy the png_struct it used
// for parsing so it can use a modified IHDR.
TEST(AnimatedPNGTests, SubsetFromIHDR) {
  const char* pngFile =
      "/LayoutTests/images/resources/"
      "png-animated-idat-not-part-of-animation.png";
  auto originalData = readFile(pngFile);
  ASSERT_FALSE(originalData->isEmpty());

  const size_t fcTLOffset = 2519u;
  auto data = SharedBuffer::create(originalData->data(), fcTLOffset);

  const size_t fcTLSize = 38u;
  png_byte fcTL[fcTLSize];
  memcpy(fcTL, originalData->data() + fcTLOffset, fcTLSize);
  // Modify to have a subset frame (yOffset 1, height 34 out of 35).
  writeUint32(34, fcTL + 16u);
  writeUint32(1, fcTL + 24u);
  writeUint32(3972842751, fcTL + 34u);
  data->append(reinterpret_cast<const char*>(fcTL), fcTLSize);

  // Append the rest of the data.
  // Note: If PNGImageDecoder changes to reject an image with too many
  // rows, the fdAT data will need to be modified as well.
  data->append(originalData->data() + fcTLOffset + fcTLSize,
               originalData->size() - data->size());
  ASSERT_EQ(originalData->size(), data->size());

  // This will test both byte by byte and using the full data, and compare.
  testByteByByteDecode(createDecoder, data.get(), 1, cAnimationNone);
}

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

TEST(StaticPNGTests, InvalidIHDRChunk) {
  testMissingDataBreaksDecoding("/LayoutTests/images/resources/png-simple.png",
                                20u, 2u);
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
      1u);   // expected frame count before failure
}

TEST(PNGTests, VerifyFrameCompleteBehavior) {
  struct {
    const char* name;
    size_t expectedFrameCount;
    size_t offsetInFirstFrame;
  } gRecs[] = {
      {"/LayoutTests/images/resources/"
       "png-animated-three-independent-frames.png",
       3u, 150u},
      {"/LayoutTests/images/resources/"
       "png-animated-idat-part-of-animation.png",
       4u, 160u},

      {"/LayoutTests/images/resources/png-simple.png", 1u, 700u},
      {"/LayoutTests/images/resources/lenna.png", 1u, 40000u},
  };
  for (const auto& rec : gRecs) {
    auto fullData = readFile(rec.name);
    ASSERT_TRUE(fullData.get());

    // Create with enough data for part of the first frame.
    auto decoder = createDecoder();
    auto data = SharedBuffer::create(fullData->data(), rec.offsetInFirstFrame);
    decoder->setData(data.get(), false);

    EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));

    // Parsing the size is not enough to mark the frame as complete.
    EXPECT_TRUE(decoder->isSizeAvailable());
    EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));

    const auto partialFrameCount = decoder->frameCount();
    EXPECT_EQ(1u, partialFrameCount);

    // Frame is not complete, even after decoding partially.
    EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));
    auto* frame = decoder->frameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_NE(ImageFrame::FrameComplete, frame->getStatus());
    EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));

    decoder->setData(fullData.get(), true);

    // With full data, parsing the size still does not mark a frame as
    // complete.
    EXPECT_TRUE(decoder->isSizeAvailable());
    EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));

    const auto frameCount = decoder->frameCount();
    ASSERT_EQ(rec.expectedFrameCount, frameCount);

    if (frameCount > 1u) {
      // After parsing (the full file), all frames are complete.
      for (size_t i = 0; i < frameCount; ++i)
        EXPECT_TRUE(decoder->frameIsCompleteAtIndex(i));
    } else {
      // A single frame image is not reported complete until decoding.
      EXPECT_FALSE(decoder->frameIsCompleteAtIndex(0));
    }

    frame = decoder->frameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_EQ(ImageFrame::FrameComplete, frame->getStatus());
    EXPECT_TRUE(decoder->frameIsCompleteAtIndex(0));
  }
}

TEST(PNGTests, sizeMayOverflow) {
  auto decoder =
      createDecoderWithPngData("/LayoutTests/images/resources/crbug702934.png");
  EXPECT_FALSE(decoder->isSizeAvailable());
  EXPECT_TRUE(decoder->failed());
}

};  // namespace blink
