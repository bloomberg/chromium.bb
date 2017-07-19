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

#include "platform/image-decoders/gif/GIFImageDecoder.h"

#include <memory>
#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoderTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char kLayoutTestResourcesDir[] = "LayoutTests/images/resources";

std::unique_ptr<ImageDecoder> CreateDecoder() {
  return WTF::WrapUnique(
      new GIFImageDecoder(ImageDecoder::kAlphaNotPremultiplied,
                          ColorBehavior::TransformToTargetForTesting(),
                          ImageDecoder::kNoDecodedImageByteLimit));
}

void TestRepetitionCount(const char* dir,
                         const char* file,
                         int expected_repetition_count) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();
  RefPtr<SharedBuffer> data = ReadFile(dir, file);
  ASSERT_TRUE(data.Get());
  decoder->SetData(data.Get(), true);
  EXPECT_EQ(kAnimationLoopOnce,
            decoder->RepetitionCount());  // Default value before decode.

  for (size_t i = 0; i < decoder->FrameCount(); ++i) {
    ImageFrame* frame = decoder->FrameBufferAtIndex(i);
    EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  }

  EXPECT_EQ(expected_repetition_count,
            decoder->RepetitionCount());  // Expected value after decode.
}

}  // anonymous namespace

TEST(GIFImageDecoderTest, decodeTwoFrames) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  RefPtr<SharedBuffer> data = ReadFile(kLayoutTestResourcesDir, "animated.gif");
  ASSERT_TRUE(data.Get());
  decoder->SetData(data.Get(), true);
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());

  ImageFrame* frame = decoder->FrameBufferAtIndex(0);
  uint32_t generation_id0 = frame->Bitmap().getGenerationID();
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());

  frame = decoder->FrameBufferAtIndex(1);
  uint32_t generation_id1 = frame->Bitmap().getGenerationID();
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());
  EXPECT_TRUE(generation_id0 != generation_id1);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecode) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  RefPtr<SharedBuffer> data = ReadFile(kLayoutTestResourcesDir, "animated.gif");
  ASSERT_TRUE(data.Get());
  decoder->SetData(data.Get(), true);
  EXPECT_EQ(kAnimationLoopOnce, decoder->RepetitionCount());

  // This call will parse the entire file.
  EXPECT_EQ(2u, decoder->FrameCount());

  ImageFrame* frame = decoder->FrameBufferAtIndex(0);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());

  frame = decoder->FrameBufferAtIndex(1);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_EQ(16, frame->Bitmap().width());
  EXPECT_EQ(16, frame->Bitmap().height());
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, parseByteByByte) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  const Vector<char> data =
      ReadFile(kLayoutTestResourcesDir, "animated.gif")->Copy();

  size_t frame_count = 0;

  // Pass data to decoder byte by byte.
  for (size_t length = 1; length <= data.size(); ++length) {
    RefPtr<SharedBuffer> temp_data = SharedBuffer::Create(data.data(), length);
    decoder->SetData(temp_data.Get(), length == data.size());

    EXPECT_LE(frame_count, decoder->FrameCount());
    frame_count = decoder->FrameCount();
  }

  EXPECT_EQ(2u, decoder->FrameCount());

  decoder->FrameBufferAtIndex(0);
  decoder->FrameBufferAtIndex(1);
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, parseAndDecodeByteByByte) {
  TestByteByByteDecode(&CreateDecoder, kLayoutTestResourcesDir,
                       "animated-gif-with-offsets.gif", 5u,
                       kAnimationLoopInfinite);
}

TEST(GIFImageDecoderTest, brokenSecondFrame) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  RefPtr<SharedBuffer> data = ReadFile(kDecodersTestingDir, "broken.gif");
  ASSERT_TRUE(data.Get());
  decoder->SetData(data.Get(), true);

  // One frame is detected but cannot be decoded.
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->FrameBufferAtIndex(1);
  EXPECT_FALSE(frame);
}

TEST(GIFImageDecoderTest, progressiveDecode) {
  TestProgressiveDecoding(&CreateDecoder, kDecodersTestingDir, "radient.gif");
}

TEST(GIFImageDecoderTest, allDataReceivedTruncation) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  const Vector<char> data =
      ReadFile(kLayoutTestResourcesDir, "animated.gif")->Copy();

  ASSERT_GE(data.size(), 10u);
  RefPtr<SharedBuffer> temp_data =
      SharedBuffer::Create(data.data(), data.size() - 10);
  decoder->SetData(temp_data.Get(), true);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());

  decoder->FrameBufferAtIndex(0);
  EXPECT_FALSE(decoder->Failed());
  decoder->FrameBufferAtIndex(1);
  EXPECT_TRUE(decoder->Failed());
}

TEST(GIFImageDecoderTest, frameIsComplete) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  RefPtr<SharedBuffer> data = ReadFile(kLayoutTestResourcesDir, "animated.gif");
  ASSERT_TRUE(data.Get());
  decoder->SetData(data.Get(), true);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(1));
  EXPECT_EQ(kAnimationLoopInfinite, decoder->RepetitionCount());
}

TEST(GIFImageDecoderTest, frameIsCompleteLoading) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  RefPtr<SharedBuffer> data_buffer =
      ReadFile(kLayoutTestResourcesDir, "animated.gif");
  ASSERT_TRUE(data_buffer.Get());
  const Vector<char> data = data_buffer->Copy();

  ASSERT_GE(data.size(), 10u);
  RefPtr<SharedBuffer> temp_data =
      SharedBuffer::Create(data.data(), data.size() - 10);
  decoder->SetData(temp_data.Get(), false);

  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_FALSE(decoder->FrameIsReceivedAtIndex(1));

  decoder->SetData(data_buffer.Get(), true);
  EXPECT_EQ(2u, decoder->FrameCount());
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(decoder->FrameIsReceivedAtIndex(1));
}

TEST(GIFImageDecoderTest, badTerminator) {
  RefPtr<SharedBuffer> reference_data =
      ReadFile(kDecodersTestingDir, "radient.gif");
  RefPtr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "radient-bad-terminator.gif");
  ASSERT_TRUE(reference_data.Get());
  ASSERT_TRUE(test_data.Get());

  std::unique_ptr<ImageDecoder> reference_decoder = CreateDecoder();
  reference_decoder->SetData(reference_data.Get(), true);
  EXPECT_EQ(1u, reference_decoder->FrameCount());
  ImageFrame* reference_frame = reference_decoder->FrameBufferAtIndex(0);
  DCHECK(reference_frame);

  std::unique_ptr<ImageDecoder> test_decoder = CreateDecoder();
  test_decoder->SetData(test_data.Get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ImageFrame* test_frame = test_decoder->FrameBufferAtIndex(0);
  DCHECK(test_frame);

  EXPECT_EQ(HashBitmap(reference_frame->Bitmap()),
            HashBitmap(test_frame->Bitmap()));
}

TEST(GIFImageDecoderTest, updateRequiredPreviousFrameAfterFirstDecode) {
  TestUpdateRequiredPreviousFrameAfterFirstDecode(
      &CreateDecoder, kLayoutTestResourcesDir, "animated-10color.gif");
}

TEST(GIFImageDecoderTest, randomFrameDecode) {
  // Single frame image.
  TestRandomFrameDecode(&CreateDecoder, kDecodersTestingDir, "radient.gif");
  // Multiple frame images.
  TestRandomFrameDecode(&CreateDecoder, kLayoutTestResourcesDir,
                        "animated-gif-with-offsets.gif");
  TestRandomFrameDecode(&CreateDecoder, kLayoutTestResourcesDir,
                        "animated-10color.gif");
}

TEST(GIFImageDecoderTest, randomDecodeAfterClearFrameBufferCache) {
  // Single frame image.
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kDecodersTestingDir, "radient.gif");
  // Multiple frame images.
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kLayoutTestResourcesDir, "animated-gif-with-offsets.gif");
  TestRandomDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kLayoutTestResourcesDir, "animated-10color.gif");
}

TEST(GIFImageDecoderTest, resumePartialDecodeAfterClearFrameBufferCache) {
  TestResumePartialDecodeAfterClearFrameBufferCache(
      &CreateDecoder, kLayoutTestResourcesDir, "animated-10color.gif");
}

// The first LZW codes in the image are invalid values that try to create a loop
// in the dictionary. Decoding should fail, but not infinitely loop or corrupt
// memory.
TEST(GIFImageDecoderTest, badInitialCode) {
  RefPtr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "bad-initial-code.gif");
  ASSERT_TRUE(test_data.Get());

  std::unique_ptr<ImageDecoder> test_decoder = CreateDecoder();
  test_decoder->SetData(test_data.Get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ASSERT_TRUE(test_decoder->FrameBufferAtIndex(0));
  EXPECT_TRUE(test_decoder->Failed());
}

// The image has an invalid LZW code that exceeds dictionary size. Decoding
// should fail.
TEST(GIFImageDecoderTest, badCode) {
  RefPtr<SharedBuffer> test_data =
      ReadFile(kDecodersTestingDir, "bad-code.gif");
  ASSERT_TRUE(test_data.Get());

  std::unique_ptr<ImageDecoder> test_decoder = CreateDecoder();
  test_decoder->SetData(test_data.Get(), true);
  EXPECT_EQ(1u, test_decoder->FrameCount());
  ASSERT_TRUE(test_decoder->FrameBufferAtIndex(0));
  EXPECT_TRUE(test_decoder->Failed());
}

TEST(GIFImageDecoderTest, invalidDisposalMethod) {
  std::unique_ptr<ImageDecoder> decoder = CreateDecoder();

  // The image has 2 frames, with disposal method 4 and 5, respectively.
  RefPtr<SharedBuffer> data =
      ReadFile(kDecodersTestingDir, "invalid-disposal-method.gif");
  ASSERT_TRUE(data.Get());
  decoder->SetData(data.Get(), true);

  EXPECT_EQ(2u, decoder->FrameCount());
  // Disposal method 4 is converted to ImageFrame::DisposeOverwritePrevious.
  EXPECT_EQ(ImageFrame::kDisposeOverwritePrevious,
            decoder->FrameBufferAtIndex(0)->GetDisposalMethod());
  // Disposal method 5 is ignored.
  EXPECT_EQ(ImageFrame::kDisposeNotSpecified,
            decoder->FrameBufferAtIndex(1)->GetDisposalMethod());
}

TEST(GIFImageDecoderTest, firstFrameHasGreaterSizeThanScreenSize) {
  const Vector<char> full_data =
      ReadFile(kDecodersTestingDir,
               "first-frame-has-greater-size-than-screen-size.gif")
          ->Copy();

  std::unique_ptr<ImageDecoder> decoder;
  IntSize frame_size;

  // Compute hashes when the file is truncated.
  for (size_t i = 1; i <= full_data.size(); ++i) {
    decoder = CreateDecoder();
    RefPtr<SharedBuffer> data = SharedBuffer::Create(full_data.data(), i);
    decoder->SetData(data.Get(), i == full_data.size());

    if (decoder->IsSizeAvailable() && !frame_size.Width() &&
        !frame_size.Height()) {
      frame_size = decoder->DecodedSize();
      continue;
    }

    ASSERT_EQ(frame_size.Width(), decoder->DecodedSize().Width());
    ASSERT_EQ(frame_size.Height(), decoder->DecodedSize().Height());
  }
}

TEST(GIFImageDecoderTest, verifyRepetitionCount) {
  TestRepetitionCount(kLayoutTestResourcesDir, "full2loop.gif", 2);
  TestRepetitionCount(kDecodersTestingDir, "radient.gif", kAnimationNone);
}

TEST(GIFImageDecoderTest, bitmapAlphaType) {
  RefPtr<SharedBuffer> full_data_buffer =
      ReadFile(kDecodersTestingDir, "radient.gif");
  ASSERT_TRUE(full_data_buffer.Get());
  const Vector<char> full_data = full_data_buffer->Copy();

  // Empirically chosen truncation size:
  //   a) large enough to produce a partial frame &&
  //   b) small enough to not fully decode the frame
  const size_t kTruncateSize = 800;
  ASSERT_TRUE(kTruncateSize < full_data.size());
  RefPtr<SharedBuffer> partial_data =
      SharedBuffer::Create(full_data.data(), kTruncateSize);

  std::unique_ptr<ImageDecoder> premul_decoder = WTF::WrapUnique(
      new GIFImageDecoder(ImageDecoder::kAlphaPremultiplied,
                          ColorBehavior::TransformToTargetForTesting(),
                          ImageDecoder::kNoDecodedImageByteLimit));
  std::unique_ptr<ImageDecoder> unpremul_decoder = WTF::WrapUnique(
      new GIFImageDecoder(ImageDecoder::kAlphaNotPremultiplied,
                          ColorBehavior::TransformToTargetForTesting(),
                          ImageDecoder::kNoDecodedImageByteLimit));

  // Partially decoded frame => the frame alpha type is unknown and should
  // reflect the requested format.
  premul_decoder->SetData(partial_data.Get(), false);
  ASSERT_TRUE(premul_decoder->FrameCount());
  unpremul_decoder->SetData(partial_data.Get(), false);
  ASSERT_TRUE(unpremul_decoder->FrameCount());
  ImageFrame* premul_frame = premul_decoder->FrameBufferAtIndex(0);
  EXPECT_TRUE(premul_frame &&
              premul_frame->GetStatus() != ImageFrame::kFrameComplete);
  EXPECT_EQ(premul_frame->Bitmap().alphaType(), kPremul_SkAlphaType);
  ImageFrame* unpremul_frame = unpremul_decoder->FrameBufferAtIndex(0);
  EXPECT_TRUE(unpremul_frame &&
              unpremul_frame->GetStatus() != ImageFrame::kFrameComplete);
  EXPECT_EQ(unpremul_frame->Bitmap().alphaType(), kUnpremul_SkAlphaType);

  // Fully decoded frame => the frame alpha type is known (opaque).
  premul_decoder->SetData(full_data_buffer.Get(), true);
  ASSERT_TRUE(premul_decoder->FrameCount());
  unpremul_decoder->SetData(full_data_buffer.Get(), true);
  ASSERT_TRUE(unpremul_decoder->FrameCount());
  premul_frame = premul_decoder->FrameBufferAtIndex(0);
  EXPECT_TRUE(premul_frame &&
              premul_frame->GetStatus() == ImageFrame::kFrameComplete);
  EXPECT_EQ(premul_frame->Bitmap().alphaType(), kOpaque_SkAlphaType);
  unpremul_frame = unpremul_decoder->FrameBufferAtIndex(0);
  EXPECT_TRUE(unpremul_frame &&
              unpremul_frame->GetStatus() == ImageFrame::kFrameComplete);
  EXPECT_EQ(unpremul_frame->Bitmap().alphaType(), kOpaque_SkAlphaType);
}

namespace {
// Needed to exercise ImageDecoder::SetMemoryAllocator, but still does the
// default allocation.
class Allocator final : public SkBitmap::Allocator {
  bool allocPixelRef(SkBitmap* dst) override { return dst->tryAllocPixels(); }
};
}

// Ensure that calling SetMemoryAllocator does not short-circuit
// InitializeNewFrame.
TEST(GIFImageDecoderTest, externalAllocator) {
  auto data = ReadFile(kLayoutTestResourcesDir, "boston.gif");
  ASSERT_TRUE(data.Get());

  auto decoder = CreateDecoder();
  decoder->SetData(data.Get(), true);

  Allocator allocator;
  decoder->SetMemoryAllocator(&allocator);
  EXPECT_EQ(1u, decoder->FrameCount());
  ImageFrame* frame = decoder->FrameBufferAtIndex(0);
  decoder->SetMemoryAllocator(nullptr);

  ASSERT_TRUE(frame);
  EXPECT_EQ(IntRect(IntPoint(), decoder->Size()), frame->OriginalFrameRect());
  EXPECT_FALSE(frame->HasAlpha());
}

}  // namespace blink
