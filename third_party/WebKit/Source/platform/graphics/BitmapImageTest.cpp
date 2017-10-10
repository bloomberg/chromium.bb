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
#include "platform/graphics/test/MockImageDecoder.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
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

  static RefPtr<SharedBuffer> ReadFile(const char* file_name) {
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
  void StartAnimation() { image_->StartAnimation(); }

  void SetFirstFrameNotComplete() { image_->frames_[0].is_complete_ = false; }

  void LoadImage(const char* file_name, bool load_all_frames = true) {
    RefPtr<SharedBuffer> image_data = ReadFile(file_name);
    ASSERT_TRUE(image_data.get());

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

  int RepetitionCount() { return image_->RepetitionCount(); }

  int AnimationFinished() { return image_->animation_finished_; }

  RefPtr<Image> ImageForDefaultFrame() {
    return image_->ImageForDefaultFrame();
  }

  int LastDecodedSizeChange() {
    return image_observer_->last_decoded_size_changed_delta_;
  }

  RefPtr<SharedBuffer> Data() { return image_->Data(); }

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
  ASSERT_TRUE(image_data.get());

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

TEST_F(BitmapImageTest, ConstantImageIdForPartiallyLoadedImages) {
  RefPtr<SharedBuffer> image_data =
      ReadFile("/LayoutTests/images/resources/green.jpg");
  ASSERT_TRUE(image_data.get());

  // Create a new buffer to partially supply the data.
  RefPtr<SharedBuffer> partial_buffer = SharedBuffer::Create();
  partial_buffer->Append(image_data->Data(), image_data->size() - 4);

  // First partial load. Repeated calls for a PaintImage should have the same
  // image until the data changes or the decoded data is destroyed.
  ASSERT_EQ(image_->SetData(partial_buffer, false), Image::kSizeAvailable);
  auto image1 = image_->PaintImageForCurrentFrame();
  auto image2 = image_->PaintImageForCurrentFrame();
  EXPECT_EQ(image1, image2);
  auto sk_image1 = image1.GetSkImage();
  auto sk_image2 = image2.GetSkImage();
  EXPECT_EQ(sk_image1->uniqueID(), sk_image2->uniqueID());

  // Frame keys should be the same for these PaintImages.
  EXPECT_EQ(image1.GetKeyForFrame(image1.frame_index()),
            image2.GetKeyForFrame(image2.frame_index()));
  EXPECT_EQ(image1.frame_index(), 0u);
  EXPECT_EQ(image2.frame_index(), 0u);

  // Destroy the decoded data. This generates a new id since we don't cache
  // image ids for partial decodes.
  DestroyDecodedData();
  auto image3 = image_->PaintImageForCurrentFrame();
  auto sk_image3 = image3.GetSkImage();
  EXPECT_NE(sk_image1, sk_image3);
  EXPECT_NE(sk_image1->uniqueID(), sk_image3->uniqueID());

  // Since the cached generator is discarded on destroying the cached decode,
  // the new content id is generated resulting in an updated frame key.
  EXPECT_NE(image1.GetKeyForFrame(image1.frame_index()),
            image3.GetKeyForFrame(image3.frame_index()));
  EXPECT_EQ(image3.frame_index(), 0u);

  // Load complete. This should generate a new image id.
  image_->SetData(image_data, true);
  auto complete_image = image_->PaintImageForCurrentFrame();
  auto complete_sk_image = complete_image.GetSkImage();
  EXPECT_NE(sk_image3, complete_sk_image);
  EXPECT_NE(sk_image3->uniqueID(), complete_sk_image->uniqueID());
  EXPECT_NE(complete_image.GetKeyForFrame(complete_image.frame_index()),
            image3.GetKeyForFrame(image3.frame_index()));
  EXPECT_EQ(complete_image.frame_index(), 0u);

  // Destroy the decoded data and re-create the PaintImage. The frame key
  // remains constant but the SkImage id will change since we don't cache skia
  // uniqueIDs.
  DestroyDecodedData();
  auto new_complete_image = image_->PaintImageForCurrentFrame();
  auto new_complete_sk_image = new_complete_image.GetSkImage();
  EXPECT_NE(new_complete_sk_image, complete_sk_image);
  EXPECT_EQ(new_complete_image.GetKeyForFrame(new_complete_image.frame_index()),
            complete_image.GetKeyForFrame(complete_image.frame_index()));
  EXPECT_EQ(new_complete_image.frame_index(), 0u);
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

class BitmapImageTestWithMockDecoder : public BitmapImageTest,
                                       public MockImageDecoderClient {
 public:
  void SetUp() override {
    original_time_function_ = SetTimeFunctionsForTesting([] { return now_; });

    BitmapImageTest::SetUp();
    auto decoder = MockImageDecoder::Create(this);
    decoder->SetSize(10, 10);
    image_->SetDecoderForTesting(
        DeferredImageDecoder::CreateForTesting(std::move(decoder)));
  }

  void TearDown() override {
    SetTimeFunctionsForTesting(original_time_function_);
  }

  void DecoderBeingDestroyed() override {}
  void DecodeRequested() override {}
  ImageFrame::Status GetStatus(size_t index) override {
    if (index < frame_count_ - 1 || last_frame_complete_)
      return ImageFrame::Status::kFrameComplete;
    return ImageFrame::Status::kFramePartial;
  }
  size_t FrameCount() override { return frame_count_; }
  int RepetitionCount() const override { return repetition_count_; }
  TimeDelta FrameDuration() const override { return duration_; }

 protected:
  TimeDelta duration_;
  int repetition_count_;
  size_t frame_count_;
  bool last_frame_complete_;

  static double now_;
  TimeFunction original_time_function_;
};

double BitmapImageTestWithMockDecoder::now_ = 0;

TEST_F(BitmapImageTestWithMockDecoder, ImageMetadataTracking) {
  // For a zero duration, we should make it non-zero when creating a PaintImage.
  repetition_count_ = kAnimationLoopOnce;
  frame_count_ = 4u;
  last_frame_complete_ = false;
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), false);

  PaintImage image = image_->PaintImageForCurrentFrame();
  ASSERT_TRUE(image);
  EXPECT_EQ(image.FrameCount(), frame_count_);
  EXPECT_EQ(image.completion_state(),
            PaintImage::CompletionState::PARTIALLY_DONE);
  EXPECT_EQ(image.repetition_count(), repetition_count_);
  for (size_t i = 0; i < image.GetFrameMetadata().size(); ++i) {
    const auto& data = image.GetFrameMetadata()[i];
    EXPECT_EQ(data.duration, base::TimeDelta::FromMilliseconds(100));
    if (i == frame_count_ - 1 && !last_frame_complete_)
      EXPECT_FALSE(data.complete);
    else
      EXPECT_TRUE(data.complete);
  }

  // Now the load is finished.
  duration_ = TimeDelta::FromSeconds(1);
  repetition_count_ = kAnimationLoopInfinite;
  frame_count_ = 6u;
  last_frame_complete_ = true;
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), true);

  image = image_->PaintImageForCurrentFrame();
  ASSERT_TRUE(image);
  EXPECT_EQ(image.FrameCount(), frame_count_);
  EXPECT_EQ(image.completion_state(), PaintImage::CompletionState::DONE);
  EXPECT_EQ(image.repetition_count(), repetition_count_);
  for (size_t i = 0; i < image.GetFrameMetadata().size(); ++i) {
    const auto& data = image.GetFrameMetadata()[i];
    if (i < 4u)
      EXPECT_EQ(data.duration, base::TimeDelta::FromMilliseconds(100));
    else
      EXPECT_EQ(data.duration, base::TimeDelta::FromSeconds(1));
    EXPECT_TRUE(data.complete);
  }
};

TEST_F(BitmapImageTestWithMockDecoder, AnimationPolicyOverride) {
  repetition_count_ = kAnimationLoopInfinite;
  frame_count_ = 4u;
  last_frame_complete_ = true;
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), false);

  PaintImage image = image_->PaintImageForCurrentFrame();
  EXPECT_EQ(image.repetition_count(), repetition_count_);

  // Only one loop allowed.
  image_->SetAnimationPolicy(kImageAnimationPolicyAnimateOnce);
  image = image_->PaintImageForCurrentFrame();
  EXPECT_EQ(image.repetition_count(), kAnimationLoopOnce);

  // No animation allowed.
  image_->SetAnimationPolicy(kImageAnimationPolicyNoAnimation);
  image = image_->PaintImageForCurrentFrame();
  EXPECT_EQ(image.repetition_count(), kAnimationNone);

  // Default policy.
  image_->SetAnimationPolicy(kImageAnimationPolicyAllowed);
  image = image_->PaintImageForCurrentFrame();
  EXPECT_EQ(image.repetition_count(), repetition_count_);
}

TEST_F(BitmapImageTestWithMockDecoder, DontAdvanceToIncompleteFrame) {
  RuntimeEnabledFeatures::SetCompositorImageAnimationsEnabled(false);

  repetition_count_ = kAnimationLoopOnce;
  frame_count_ = 3u;
  last_frame_complete_ = false;
  duration_ = TimeDelta::FromSeconds(10);
  now_ = 10;

  // Last frame of the image is incomplete for a completely loaded image. We
  // still won't advance it.
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), true);

  RefPtr<scheduler::FakeWebTaskRunner> task_runner =
      WTF::AdoptRef(new scheduler::FakeWebTaskRunner);
  image_->SetTaskRunnerForTesting(task_runner);
  task_runner->SetTime(10);

  // Start the animation at 10s. This should schedule a timer for the next frame
  // after 10 seconds.
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 0u);

  // Run the timer task, image advanced to the second frame.
  now_ = 20;
  task_runner->AdvanceTimeAndRun(10);
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 1u);

  // Go past the desired time for the next frame, call StartAnimation. The frame
  // still isn't advanced because its incomplete.
  now_ = 45;
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 1u);
}

TEST_F(BitmapImageTestWithMockDecoder, FrameSkipTracking) {
  RuntimeEnabledFeatures::SetCompositorImageAnimationsEnabled(false);

  repetition_count_ = kAnimationLoopOnce;
  frame_count_ = 7u;
  last_frame_complete_ = false;
  duration_ = TimeDelta::FromSeconds(10);
  now_ = 10;

  // Start with an image that is incomplete, and the last frame is not fully
  // received.
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), false);

  RefPtr<scheduler::FakeWebTaskRunner> task_runner =
      WTF::AdoptRef(new scheduler::FakeWebTaskRunner);
  image_->SetTaskRunnerForTesting(task_runner);
  task_runner->SetTime(10);

  // Start the animation at 10s. This should schedule a timer for the next frame
  // after 10 seconds.
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 0u);

  // No frames skipped since we just started the animation.
  EXPECT_EQ(image_->last_num_frames_skipped_for_testing().value(), 0u);

  // Advance the time to 15s. The frame is still at 0u because the posted task
  // should run at 20s.
  task_runner->AdvanceTimeAndRun(5);
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 0u);

  // Advance the time to 20s. The task runs and advances the animation forward
  // to 1u.
  task_runner->AdvanceTimeAndRun(5);
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 1u);
  EXPECT_EQ(image_->last_num_frames_skipped_for_testing().value(), 0u);

  // Set now_ to 41 seconds. Since the animation started at 10s, and each frame
  // has a duration of 10s, we should see the fourth frame at 41 seconds.
  now_ = 41;
  task_runner->SetTime(41);
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 3u);
  EXPECT_EQ(image_->last_num_frames_skipped_for_testing().value(), 1u);

  // We should have scheduled a task to move to the fifth frame in
  // StartAnimation above, at 50s.
  // Advance by 5s, not time for the next frame yet.
  task_runner->AdvanceTimeAndRun(5);
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 3u);

  // Advance to the fifth frame.
  task_runner->SetTime(50);
  task_runner->AdvanceTimeAndRun(0);
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 4u);

  // At 70s, we would want to display the last frame and would skip 1 frame.
  // But because its incomplete, we advanced to the sixth frame and did not need
  // to skip anything.
  now_ = 71;
  task_runner->SetTime(71);
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 5u);
  EXPECT_EQ(image_->last_num_frames_skipped_for_testing().value(), 0u);

  // Run any pending tasks and try to animate again. Can't advance the animation
  // because the last frame is not complete.
  task_runner->AdvanceTimeAndRun(0);
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 5u);
  EXPECT_FALSE(image_->last_num_frames_skipped_for_testing().has_value());

  // Finish the load and kick the animation again. It finishes during catch up.
  // But no frame skipped because we just advanced to the last frame.
  last_frame_complete_ = true;
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), true);
  StartAnimation();
  EXPECT_EQ(image_->PaintImageForCurrentFrame().frame_index(), 6u);
  EXPECT_EQ(image_->last_num_frames_skipped_for_testing().value(), 0u);
}

TEST_F(BitmapImageTestWithMockDecoder, ResetAnimation) {
  repetition_count_ = kAnimationLoopInfinite;
  frame_count_ = 4u;
  last_frame_complete_ = true;
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), false);

  PaintImage image = image_->PaintImageForCurrentFrame();
  image_->ResetAnimation();
  PaintImage image2 = image_->PaintImageForCurrentFrame();
  EXPECT_GT(image2.reset_animation_sequence_id(),
            image.reset_animation_sequence_id());
}

TEST_F(BitmapImageTestWithMockDecoder, PaintImageForStaticBitmapImage) {
  repetition_count_ = kAnimationLoopInfinite;
  frame_count_ = 5;
  last_frame_complete_ = true;
  image_->SetData(SharedBuffer::Create("data", sizeof("data")), false);

  // PaintImage for the original image is animated.
  EXPECT_TRUE(image_->PaintImageForCurrentFrame().ShouldAnimate());

  // But the StaticBitmapImage is not.
  EXPECT_FALSE(image_->ImageForDefaultFrame()
                   ->PaintImageForCurrentFrame()
                   .ShouldAnimate());
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
