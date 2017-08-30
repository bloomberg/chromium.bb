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

#include "platform/graphics/DeferredImageDecoder.h"

#include <memory>
#include "platform/CrossThreadFunctional.h"
#include "platform/SharedBuffer.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/ImageFrameGenerator.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/graphics/test/MockImageDecoder.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

// Raw data for a PNG file with 1x1 white pixels.
const unsigned char kWhitePNG[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00,
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
    0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0b, 0x13, 0x00, 0x00,
    0x0b, 0x13, 0x01, 0x00, 0x9a, 0x9c, 0x18, 0x00, 0x00, 0x00, 0x0c, 0x49,
    0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff, 0x3f, 0x00, 0x05,
    0xfe, 0x02, 0xfe, 0xdc, 0xcc, 0x59, 0xe7, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

// Raw data for a 1x1 animated GIF with 2 frames.
const unsigned char kAnimatedGIF[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0xf0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xff, 0x0b, 0x4e, 0x45,
    0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x21, 0xff, 0x0b, 0x49, 0x6d, 0x61, 0x67, 0x65, 0x4d, 0x61,
    0x67, 0x69, 0x63, 0x6b, 0x0d, 0x67, 0x61, 0x6d, 0x6d, 0x61, 0x3d, 0x30,
    0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00, 0x21, 0xff, 0x0b, 0x49, 0x6d,
    0x61, 0x67, 0x65, 0x4d, 0x61, 0x67, 0x69, 0x63, 0x6b, 0x0d, 0x67, 0x61,
    0x6d, 0x6d, 0x61, 0x3d, 0x30, 0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00,
    0x21, 0xf9, 0x04, 0x00, 0x14, 0x00, 0xff, 0x00, 0x21, 0xfe, 0x20, 0x43,
    0x72, 0x65, 0x61, 0x74, 0x65, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20,
    0x65, 0x7a, 0x67, 0x69, 0x66, 0x2e, 0x63, 0x6f, 0x6d, 0x20, 0x67, 0x69,
    0x66, 0x20, 0x6d, 0x61, 0x6b, 0x65, 0x72, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01, 0x00, 0x21,
    0xf9, 0x04, 0x00, 0x14, 0x00, 0xff, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x4c, 0x01, 0x00, 0x3b,
};

// Raw data for a GIF file with 1x1 white pixels. Modified from animatedGIF.
const unsigned char kWhiteGIF[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x01, 0x00, 0x01, 0x00, 0xf0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x21, 0xff, 0x0b, 0x4e, 0x45,
    0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2e, 0x30, 0x03, 0x01, 0x00,
    0x00, 0x00, 0x21, 0xff, 0x0b, 0x49, 0x6d, 0x61, 0x67, 0x65, 0x4d, 0x61,
    0x67, 0x69, 0x63, 0x6b, 0x0d, 0x67, 0x61, 0x6d, 0x6d, 0x61, 0x3d, 0x30,
    0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00, 0x21, 0xff, 0x0b, 0x49, 0x6d,
    0x61, 0x67, 0x65, 0x4d, 0x61, 0x67, 0x69, 0x63, 0x6b, 0x0d, 0x67, 0x61,
    0x6d, 0x6d, 0x61, 0x3d, 0x30, 0x2e, 0x34, 0x35, 0x34, 0x35, 0x35, 0x00,
    0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0xff, 0x00, 0x2c, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x4c, 0x01, 0x00, 0x3b};

}  // namespace

class DeferredImageDecoderTest : public ::testing::Test,
                                 public MockImageDecoderClient {
 public:
  void SetUp() override {
    ImageDecodingStore::Instance().SetCacheLimitInBytes(1024 * 1024);
    data_ = SharedBuffer::Create(kWhitePNG, sizeof(kWhitePNG));
    frame_count_ = 1;
    std::unique_ptr<MockImageDecoder> decoder = MockImageDecoder::Create(this);
    actual_decoder_ = decoder.get();
    actual_decoder_->SetSize(1, 1);
    lazy_decoder_ = DeferredImageDecoder::CreateForTesting(std::move(decoder));
    bitmap_.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
    canvas_ = base::MakeUnique<cc::SkiaPaintCanvas>(bitmap_);
    decode_request_count_ = 0;
    repetition_count_ = kAnimationNone;
    status_ = ImageFrame::kFrameComplete;
    frame_duration_ = 0;
    decoded_size_ = actual_decoder_->Size();
  }

  void TearDown() override { ImageDecodingStore::Instance().Clear(); }

  void DecoderBeingDestroyed() override { actual_decoder_ = 0; }

  void DecodeRequested() override { ++decode_request_count_; }

  size_t FrameCount() override { return frame_count_; }

  int RepetitionCount() const override { return repetition_count_; }

  ImageFrame::Status GetStatus() override { return status_; }

  float FrameDuration() const override { return frame_duration_; }

  IntSize DecodedSize() const override { return decoded_size_; }

  PaintImage CreatePaintImageAtIndex(
      size_t index,
      PaintImage::CompletionState state = PaintImage::CompletionState::DONE) {
    return CreatePaintImageAtIndex(lazy_decoder_.get(), index, state);
  }

  PaintImage CreatePaintImageAtIndex(
      DeferredImageDecoder* decoder,
      size_t index,
      PaintImage::CompletionState state = PaintImage::CompletionState::DONE) {
    return PaintImageBuilder()
        .set_id(PaintImage::GetNextId())
        .set_completion_state(state)
        .set_paint_image_generator(decoder->CreateGeneratorAtIndex(index))
        .TakePaintImage();
  }

 protected:
  void UseMockImageDecoderFactory() {
    lazy_decoder_->FrameGenerator()->SetImageDecoderFactory(
        MockImageDecoderFactory::Create(this, decoded_size_));
  }

  // Don't own this but saves the pointer to query states.
  MockImageDecoder* actual_decoder_;
  std::unique_ptr<DeferredImageDecoder> lazy_decoder_;
  SkBitmap bitmap_;
  std::unique_ptr<cc::PaintCanvas> canvas_;
  int decode_request_count_;
  RefPtr<SharedBuffer> data_;
  size_t frame_count_;
  int repetition_count_;
  ImageFrame::Status status_;
  float frame_duration_;
  IntSize decoded_size_;
};

TEST_F(DeferredImageDecoderTest, drawIntoPaintRecord) {
  lazy_decoder_->SetData(data_, true);
  PaintImage image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_EQ(1, image.width());
  EXPECT_EQ(1, image.height());

  PaintRecorder recorder;
  PaintCanvas* temp_canvas = recorder.beginRecording(100, 100);
  temp_canvas->drawImage(image, 0, 0);
  sk_sp<PaintRecord> record = recorder.finishRecordingAsPicture();
  EXPECT_EQ(0, decode_request_count_);

  canvas_->drawPicture(record);
  EXPECT_EQ(0, decode_request_count_);
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), bitmap_.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, drawIntoPaintRecordProgressive) {
  RefPtr<SharedBuffer> partial_data =
      SharedBuffer::Create(data_->Data(), data_->size() - 10);

  // Received only half the file.
  lazy_decoder_->SetData(partial_data, false);
  PaintRecorder recorder;
  PaintCanvas* temp_canvas = recorder.beginRecording(100, 100);
  PaintImage image =
      CreatePaintImageAtIndex(0, PaintImage::CompletionState::PARTIALLY_DONE);
  temp_canvas->drawImage(image, 0, 0);
  canvas_->drawPicture(recorder.finishRecordingAsPicture());

  // Fully received the file and draw the PaintRecord again.
  lazy_decoder_->SetData(data_, true);
  image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  temp_canvas = recorder.beginRecording(100, 100);
  temp_canvas->drawImage(image, 0, 0);
  canvas_->drawPicture(recorder.finishRecordingAsPicture());
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), bitmap_.getColor(0, 0));
}

static void RasterizeMain(PaintCanvas* canvas, sk_sp<PaintRecord> record) {
  canvas->drawPicture(record);
}

TEST_F(DeferredImageDecoderTest, decodeOnOtherThread) {
  lazy_decoder_->SetData(data_, true);
  PaintImage image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_EQ(1, image.width());
  EXPECT_EQ(1, image.height());

  PaintRecorder recorder;
  PaintCanvas* temp_canvas = recorder.beginRecording(100, 100);
  temp_canvas->drawImage(image, 0, 0);
  sk_sp<PaintRecord> record = recorder.finishRecordingAsPicture();
  EXPECT_EQ(0, decode_request_count_);

  // Create a thread to rasterize PaintRecord.
  std::unique_ptr<WebThread> thread =
      Platform::Current()->CreateThread("RasterThread");
  thread->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&RasterizeMain, CrossThreadUnretained(canvas_.get()),
                      record));
  thread.reset();
  EXPECT_EQ(0, decode_request_count_);
  EXPECT_EQ(SkColorSetARGB(255, 255, 255, 255), bitmap_.getColor(0, 0));
}

TEST_F(DeferredImageDecoderTest, singleFrameImageLoading) {
  status_ = ImageFrame::kFramePartial;
  lazy_decoder_->SetData(data_, false);
  EXPECT_FALSE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  PaintImage image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_FALSE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(actual_decoder_);

  status_ = ImageFrame::kFrameComplete;
  data_->Append(" ", 1u);
  lazy_decoder_->SetData(data_, true);
  EXPECT_FALSE(actual_decoder_);
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(0));

  image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_FALSE(decode_request_count_);
}

TEST_F(DeferredImageDecoderTest, multiFrameImageLoading) {
  repetition_count_ = 10;
  frame_count_ = 1;
  frame_duration_ = 10;
  status_ = ImageFrame::kFramePartial;
  lazy_decoder_->SetData(data_, false);

  PaintImage image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_FALSE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  // Anything below .011f seconds is rounded to 0.1f seconds.
  // See the implementaiton for details.
  EXPECT_FLOAT_EQ(0.1f, lazy_decoder_->FrameDurationAtIndex(0));

  frame_count_ = 2;
  frame_duration_ = 20;
  status_ = ImageFrame::kFrameComplete;
  data_->Append(" ", 1u);
  lazy_decoder_->SetData(data_, false);

  image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(1));
  EXPECT_FLOAT_EQ(0.02f, lazy_decoder_->FrameDurationAtIndex(1));
  EXPECT_TRUE(actual_decoder_);

  frame_count_ = 3;
  frame_duration_ = 30;
  status_ = ImageFrame::kFrameComplete;
  lazy_decoder_->SetData(data_, true);
  EXPECT_FALSE(actual_decoder_);
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(0));
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(1));
  EXPECT_TRUE(lazy_decoder_->FrameIsReceivedAtIndex(2));
  EXPECT_FLOAT_EQ(0.1f, lazy_decoder_->FrameDurationAtIndex(0));
  EXPECT_FLOAT_EQ(0.02f, lazy_decoder_->FrameDurationAtIndex(1));
  EXPECT_FLOAT_EQ(0.03f, lazy_decoder_->FrameDurationAtIndex(2));
  EXPECT_EQ(10, lazy_decoder_->RepetitionCount());
}

TEST_F(DeferredImageDecoderTest, decodedSize) {
  decoded_size_ = IntSize(22, 33);
  lazy_decoder_->SetData(data_, true);
  PaintImage image = CreatePaintImageAtIndex(0);
  ASSERT_TRUE(image);
  EXPECT_EQ(decoded_size_.Width(), image.width());
  EXPECT_EQ(decoded_size_.Height(), image.height());

  UseMockImageDecoderFactory();

  // The following code should not fail any assert.
  PaintRecorder recorder;
  PaintCanvas* temp_canvas = recorder.beginRecording(100, 100);
  temp_canvas->drawImage(image, 0, 0);
  sk_sp<PaintRecord> record = recorder.finishRecordingAsPicture();
  EXPECT_EQ(0, decode_request_count_);
  canvas_->drawPicture(record);
  EXPECT_EQ(1, decode_request_count_);
}

TEST_F(DeferredImageDecoderTest, smallerFrameCount) {
  frame_count_ = 1;
  lazy_decoder_->SetData(data_, false);
  EXPECT_EQ(frame_count_, lazy_decoder_->FrameCount());
  frame_count_ = 2;
  lazy_decoder_->SetData(data_, false);
  EXPECT_EQ(frame_count_, lazy_decoder_->FrameCount());
  frame_count_ = 0;
  lazy_decoder_->SetData(data_, true);
  EXPECT_EQ(frame_count_, lazy_decoder_->FrameCount());
}

TEST_F(DeferredImageDecoderTest, frameOpacity) {
  for (bool test_gif : {false, true}) {
    if (test_gif)
      data_ = SharedBuffer::Create(kWhiteGIF, sizeof(kWhiteGIF));

    std::unique_ptr<DeferredImageDecoder> decoder =
        DeferredImageDecoder::Create(
            data_, true, ImageDecoder::kAlphaPremultiplied,
            ColorBehavior::TransformToTargetForTesting());

    SkImageInfo pix_info = SkImageInfo::MakeN32Premul(1, 1);

    size_t row_bytes = pix_info.minRowBytes();
    size_t size = pix_info.getSafeSize(row_bytes);

    Vector<char> storage(size);
    SkPixmap pixmap(pix_info, storage.data(), row_bytes);

    // Before decoding, the frame is not known to be opaque.
    sk_sp<SkImage> frame =
        CreatePaintImageAtIndex(decoder.get(), 0).GetSkImage();
    ASSERT_TRUE(frame);
    EXPECT_FALSE(frame->isOpaque());

    // Force a lazy decode by reading pixels.
    EXPECT_TRUE(frame->readPixels(pixmap, 0, 0));

    // After decoding, the frame is known to be opaque.
    frame = CreatePaintImageAtIndex(decoder.get(), 0).GetSkImage();
    ASSERT_TRUE(frame);
    EXPECT_TRUE(frame->isOpaque());

    // Re-generating the opaque-marked frame should not fail.
    EXPECT_TRUE(frame->readPixels(pixmap, 0, 0));
  }
}

// The DeferredImageDecoder would sometimes assume that a frame was a certain
// size even if the actual decoder reported it had a size of 0 bytes. This test
// verifies that it's fixed by always checking with the actual decoder when
// creating a frame.
TEST_F(DeferredImageDecoderTest, respectActualDecoderSizeOnCreate) {
  data_ = SharedBuffer::Create(kAnimatedGIF, sizeof(kAnimatedGIF));
  frame_count_ = 2;
  ForceFirstFrameToBeEmpty();
  lazy_decoder_->SetData(data_, false);
  CreatePaintImageAtIndex(0);
  CreatePaintImageAtIndex(1);
  lazy_decoder_->SetData(data_, true);
  // Clears only the first frame (0 bytes). If DeferredImageDecoder doesn't
  // check with the actual decoder it reports 4 bytes instead.
  size_t frame_bytes_cleared = lazy_decoder_->ClearCacheExceptFrame(1);
  EXPECT_EQ(static_cast<size_t>(0), frame_bytes_cleared);
}

TEST_F(DeferredImageDecoderTest, data) {
  RefPtr<SharedBuffer> original_buffer =
      SharedBuffer::Create(data_->Data(), data_->size());
  EXPECT_EQ(original_buffer->size(), data_->size());
  lazy_decoder_->SetData(original_buffer, false);
  RefPtr<SharedBuffer> new_buffer = lazy_decoder_->Data();
  EXPECT_EQ(original_buffer->size(), new_buffer->size());
  const Vector<char> original_data = original_buffer->Copy();
  const Vector<char> new_data = new_buffer->Copy();
  EXPECT_EQ(0, std::memcmp(original_data.data(), new_data.data(),
                           new_buffer->size()));
}

}  // namespace blink
