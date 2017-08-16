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
#include "platform/graphics/BitmapImageMetrics.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/StdLibExtras.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

class BitmapImageTest : public ::testing::Test {
 public:
  class FakeImageObserver : public GarbageCollectedFinalized<FakeImageObserver>,
                            public ImageObserver {
    USING_GARBAGE_COLLECTED_MIXIN(FakeImageObserver);

   public:
    FakeImageObserver()
        : last_decoded_size_(0), last_decoded_size_changed_delta_(0) {}

    virtual void DecodedSizeChangedTo(const Image*, size_t new_size) {
      last_decoded_size_changed_delta_ =
          SafeCast<int>(new_size) - SafeCast<int>(last_decoded_size_);
      last_decoded_size_ = new_size;
    }
    bool ShouldPauseAnimation(const Image*) override { return false; }
    void AnimationAdvanced(const Image*) override {}
    void AsyncLoadCompleted(const Image*) override { NOTREACHED(); }

    virtual void ChangedInRect(const Image*, const IntRect&) {}

    size_t last_decoded_size_;
    int last_decoded_size_changed_delta_;
  };

  static PassRefPtr<SharedBuffer> ReadFile(const char* file_name) {
    String file_path = testing::BlinkRootDir();
    file_path.append(file_name);
    return testing::ReadFromFile(file_path);
  }

  // Accessors to BitmapImage's protected methods.
  void DestroyDecodedData() { image_->DestroyDecodedData(); }
  size_t FrameCount() { return image_->FrameCount(); }
  void FrameAtIndex(size_t index) { image_->FrameAtIndex(index); }
  void SetCurrentFrame(size_t frame) { image_->current_frame_index_ = frame; }
  size_t FrameDecodedSize(size_t frame) {
    return image_->frames_[frame].frame_bytes_;
  }
  size_t DecodedFramesCount() const { return image_->frames_.size(); }

  void SetFirstFrameNotComplete() { image_->frames_[0].is_complete_ = false; }

  void LoadImage(const char* file_name, bool load_all_frames = true) {
    RefPtr<SharedBuffer> image_data = ReadFile(file_name);
    ASSERT_TRUE(image_data.Get());

    image_->SetData(image_data, true);
    EXPECT_EQ(0u, DecodedSize());

    size_t frame_count = image_->FrameCount();
    if (load_all_frames) {
      for (size_t i = 0; i < frame_count; ++i)
        FrameAtIndex(i);
    }
  }

  size_t DecodedSize() {
    // In the context of this test, the following loop will give the correct
    // result, but only because the test forces all frames to be decoded in
    // loadImage() above. There is no general guarantee that frameDecodedSize()
    // is up to date. Because of how multi frame images (like GIF) work,
    // requesting one frame to be decoded may require other previous frames to
    // be decoded as well. In those cases frameDecodedSize() wouldn't return the
    // correct thing for the previous frame because the decoded size wouldn't
    // have propagated upwards to the BitmapImage frame cache.
    size_t size = 0;
    for (size_t i = 0; i < DecodedFramesCount(); ++i)
      size += FrameDecodedSize(i);
    return size;
  }

  void AdvanceAnimation() { image_->AdvanceAnimation(0); }

  int RepetitionCount() { return image_->RepetitionCount(true); }

  int AnimationFinished() { return image_->animation_finished_; }

  PassRefPtr<Image> ImageForDefaultFrame() {
    return image_->ImageForDefaultFrame();
  }

  int LastDecodedSizeChange() {
    return image_observer_->last_decoded_size_changed_delta_;
  }

  PassRefPtr<SharedBuffer> Data() { return image_->Data(); }

 protected:
  void SetUp() override {
    image_observer_ = new FakeImageObserver;
    image_ = BitmapImage::Create(image_observer_.Get());
  }

  Persistent<FakeImageObserver> image_observer_;
  RefPtr<BitmapImage> image_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

TEST_F(BitmapImageTest, destroyDecodedData) {
  LoadImage("/LayoutTests/images/resources/animated-10color.gif");
  size_t total_size = DecodedSize();
  EXPECT_GT(total_size, 0u);
  DestroyDecodedData();
  EXPECT_EQ(-static_cast<int>(total_size), LastDecodedSizeChange());
  EXPECT_EQ(0u, DecodedSize());
}

TEST_F(BitmapImageTest, maybeAnimated) {
  LoadImage("/LayoutTests/images/resources/gif-loop-count.gif");
  for (size_t i = 0; i < FrameCount(); ++i) {
    EXPECT_TRUE(image_->MaybeAnimated());
    AdvanceAnimation();
  }
  EXPECT_FALSE(image_->MaybeAnimated());
}

TEST_F(BitmapImageTest, animationRepetitions) {
  LoadImage("/LayoutTests/images/resources/full2loop.gif");
  int expected_repetition_count = 2;
  EXPECT_EQ(expected_repetition_count, RepetitionCount());

  // We actually loop once more than stored repetition count.
  for (int repeat = 0; repeat < expected_repetition_count + 1; ++repeat) {
    for (size_t i = 0; i < FrameCount(); ++i) {
      EXPECT_FALSE(AnimationFinished());
      AdvanceAnimation();
    }
  }
  EXPECT_TRUE(AnimationFinished());
}

TEST_F(BitmapImageTest, isAllDataReceived) {
  RefPtr<SharedBuffer> image_data =
      ReadFile("/LayoutTests/images/resources/green.jpg");
  ASSERT_TRUE(image_data.Get());

  RefPtr<BitmapImage> image = BitmapImage::Create();
  EXPECT_FALSE(image->IsAllDataReceived());

  image->SetData(image_data, false);
  EXPECT_FALSE(image->IsAllDataReceived());

  image->SetData(image_data, true);
  EXPECT_TRUE(image->IsAllDataReceived());

  image->SetData(SharedBuffer::Create("data", sizeof("data")), false);
  EXPECT_FALSE(image->IsAllDataReceived());

  image->SetData(image_data, true);
  EXPECT_TRUE(image->IsAllDataReceived());
}

TEST_F(BitmapImageTest, noColorProfile) {
  LoadImage("/LayoutTests/images/resources/green.jpg");
  EXPECT_EQ(1u, DecodedFramesCount());
  EXPECT_EQ(1024u, DecodedSize());
  EXPECT_FALSE(image_->HasColorProfile());
}

TEST_F(BitmapImageTest, jpegHasColorProfile) {
  LoadImage("/LayoutTests/images/resources/icc-v2-gbr.jpg");
  EXPECT_EQ(1u, DecodedFramesCount());
  EXPECT_EQ(227700u, DecodedSize());
  EXPECT_TRUE(image_->HasColorProfile());
}

TEST_F(BitmapImageTest, pngHasColorProfile) {
  LoadImage(
      "/LayoutTests/images/resources/"
      "palatted-color-png-gamma-one-color-profile.png");
  EXPECT_EQ(1u, DecodedFramesCount());
  EXPECT_EQ(65536u, DecodedSize());
  EXPECT_TRUE(image_->HasColorProfile());
}

TEST_F(BitmapImageTest, webpHasColorProfile) {
  LoadImage("/LayoutTests/images/resources/webp-color-profile-lossy.webp");
  EXPECT_EQ(1u, DecodedFramesCount());
  EXPECT_EQ(2560000u, DecodedSize());
  EXPECT_TRUE(image_->HasColorProfile());
}

TEST_F(BitmapImageTest, icoHasWrongFrameDimensions) {
  LoadImage("/LayoutTests/images/resources/wrong-frame-dimensions.ico");
  // This call would cause crash without fix for 408026
  ImageForDefaultFrame();
}

TEST_F(BitmapImageTest, correctDecodedDataSize) {
  // Requesting any one frame shouldn't result in decoding any other frames.
  LoadImage("/LayoutTests/images/resources/anim_none.gif", false);
  FrameAtIndex(1);
  int frame_size =
      static_cast<int>(image_->Size().Area() * sizeof(ImageFrame::PixelData));
  EXPECT_EQ(frame_size, LastDecodedSizeChange());
}

TEST_F(BitmapImageTest, recachingFrameAfterDataChanged) {
  LoadImage("/LayoutTests/images/resources/green.jpg");
  SetFirstFrameNotComplete();
  EXPECT_GT(LastDecodedSizeChange(), 0);
  image_observer_->last_decoded_size_changed_delta_ = 0;

  // Calling dataChanged causes the cache to flush, but doesn't affect the
  // source's decoded frames. It shouldn't affect decoded size.
  image_->DataChanged(true);
  EXPECT_EQ(0, LastDecodedSizeChange());
  // Recaching the first frame also shouldn't affect decoded size.
  image_->PaintImageForCurrentFrame();
  EXPECT_EQ(0, LastDecodedSizeChange());
}

TEST_F(BitmapImageTest, ConstantSkImageIdForPartiallyLoadedImages) {
  RefPtr<SharedBuffer> image_data =
      ReadFile("/LayoutTests/images/resources/green.jpg");
  ASSERT_TRUE(image_data.Get());

  // Create a new buffer to partially supply the data.
  RefPtr<SharedBuffer> partial_buffer = SharedBuffer::Create();
  partial_buffer->Append(image_data->Data(), image_data->size() - 4);

  // First partial load. Repeated calls for a PaintImage should have the same
  // image until the data changes or the decoded data is destroyed.
  ASSERT_EQ(image_->SetData(partial_buffer, false), Image::kSizeAvailable);
  auto sk_image1 = image_->PaintImageForCurrentFrame().GetSkImage();
  auto sk_image2 = image_->PaintImageForCurrentFrame().GetSkImage();
  EXPECT_EQ(sk_image1, sk_image2);

  // Destroy the decoded data. This generates a new id since we don't cache
  // image ids for partial decodes.
  DestroyDecodedData();
  auto sk_image3 = image_->PaintImageForCurrentFrame().GetSkImage();
  EXPECT_NE(sk_image1, sk_image3);
  EXPECT_NE(sk_image1->uniqueID(), sk_image3->uniqueID());

  // Load complete. This should generate a new image id.
  image_->SetData(image_data, true);
  auto complete_image = image_->PaintImageForCurrentFrame().GetSkImage();
  EXPECT_NE(sk_image3, complete_image);
  EXPECT_NE(sk_image3->uniqueID(), complete_image->uniqueID());

  // Destroy the decoded data and re-create the PaintImage. The SkImage id used
  // should remain consistent, even if a new image is created.
  DestroyDecodedData();
  auto new_complete_image = image_->PaintImageForCurrentFrame().GetSkImage();
  EXPECT_NE(new_complete_image, complete_image);
  EXPECT_EQ(new_complete_image->uniqueID(), complete_image->uniqueID());
}

TEST_F(BitmapImageTest, ImageForDefaultFrame_MultiFrame) {
  LoadImage("/LayoutTests/images/resources/anim_none.gif", false);

  // Multi-frame images create new StaticBitmapImages for each call.
  auto default_image1 = image_->ImageForDefaultFrame();
  auto default_image2 = image_->ImageForDefaultFrame();
  EXPECT_NE(default_image1, default_image2);

  // But the PaintImage should be the same.
  auto paint_image1 = default_image1->PaintImageForCurrentFrame();
  auto paint_image2 = default_image2->PaintImageForCurrentFrame();
  EXPECT_EQ(paint_image1, paint_image2);
  EXPECT_EQ(paint_image1.GetSkImage()->uniqueID(),
            paint_image2.GetSkImage()->uniqueID());
}

TEST_F(BitmapImageTest, ImageForDefaultFrame_SingleFrame) {
  LoadImage("/LayoutTests/images/resources/green.jpg");

  // Default frame images for single-frame cases is the image itself.
  EXPECT_EQ(image_->ImageForDefaultFrame(), image_);
}

template <typename HistogramEnumType>
struct HistogramTestParams {
  const char* filename;
  HistogramEnumType type;
};

template <typename HistogramEnumType>
class BitmapHistogramTest : public BitmapImageTest,
                            public ::testing::WithParamInterface<
                                HistogramTestParams<HistogramEnumType>> {
 protected:
  void RunTest(const char* histogram_name) {
    HistogramTester histogram_tester;
    LoadImage(this->GetParam().filename);
    histogram_tester.ExpectUniqueSample(histogram_name, this->GetParam().type,
                                        1);
  }
};

using DecodedImageTypeHistogramTest =
    BitmapHistogramTest<BitmapImageMetrics::DecodedImageType>;

TEST_P(DecodedImageTypeHistogramTest, ImageType) {
  RunTest("Blink.DecodedImageType");
}

const DecodedImageTypeHistogramTest::ParamType
    kDecodedImageTypeHistogramTestparams[] = {
        {"/LayoutTests/images/resources/green.jpg",
         BitmapImageMetrics::kImageJPEG},
        {"/LayoutTests/images/resources/"
         "palatted-color-png-gamma-one-color-profile.png",
         BitmapImageMetrics::kImagePNG},
        {"/LayoutTests/images/resources/animated-10color.gif",
         BitmapImageMetrics::kImageGIF},
        {"/LayoutTests/images/resources/webp-color-profile-lossy.webp",
         BitmapImageMetrics::kImageWebP},
        {"/LayoutTests/images/resources/wrong-frame-dimensions.ico",
         BitmapImageMetrics::kImageICO},
        {"/LayoutTests/images/resources/lenna.bmp",
         BitmapImageMetrics::kImageBMP}};

INSTANTIATE_TEST_CASE_P(
    DecodedImageTypeHistogramTest,
    DecodedImageTypeHistogramTest,
    ::testing::ValuesIn(kDecodedImageTypeHistogramTestparams));

using DecodedImageOrientationHistogramTest =
    BitmapHistogramTest<ImageOrientationEnum>;

TEST_P(DecodedImageOrientationHistogramTest, ImageOrientation) {
  RunTest("Blink.DecodedImage.Orientation");
}

const DecodedImageOrientationHistogramTest::ParamType
    kDecodedImageOrientationHistogramTestParams[] = {
        {"/LayoutTests/images/resources/exif-orientation-1-ul.jpg",
         kOriginTopLeft},
        {"/LayoutTests/images/resources/exif-orientation-2-ur.jpg",
         kOriginTopRight},
        {"/LayoutTests/images/resources/exif-orientation-3-lr.jpg",
         kOriginBottomRight},
        {"/LayoutTests/images/resources/exif-orientation-4-lol.jpg",
         kOriginBottomLeft},
        {"/LayoutTests/images/resources/exif-orientation-5-lu.jpg",
         kOriginLeftTop},
        {"/LayoutTests/images/resources/exif-orientation-6-ru.jpg",
         kOriginRightTop},
        {"/LayoutTests/images/resources/exif-orientation-7-rl.jpg",
         kOriginRightBottom},
        {"/LayoutTests/images/resources/exif-orientation-8-llo.jpg",
         kOriginLeftBottom}};

INSTANTIATE_TEST_CASE_P(
    DecodedImageOrientationHistogramTest,
    DecodedImageOrientationHistogramTest,
    ::testing::ValuesIn(kDecodedImageOrientationHistogramTestParams));

}  // namespace blink
