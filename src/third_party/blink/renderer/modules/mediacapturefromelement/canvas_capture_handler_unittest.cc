// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_handler.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::Test;
using ::testing::TestWithParam;

namespace blink {

namespace {

static const int kTestCanvasCaptureWidth = 320;
static const int kTestCanvasCaptureHeight = 240;
static const double kTestCanvasCaptureFramesPerSecond = 55.5;

static const int kTestCanvasCaptureFrameEvenSize = 2;
static const int kTestCanvasCaptureFrameOddSize = 3;
static const int kTestCanvasCaptureFrameColorErrorTolerance = 2;
static const int kTestAlphaValue = 175;

}  // namespace

class CanvasCaptureHandlerTest
    : public TestWithParam<testing::tuple<bool, int, int>> {
 public:
  CanvasCaptureHandlerTest() = default;

  void SetUp() override {
    canvas_capture_handler_ = CanvasCaptureHandler::CreateCanvasCaptureHandler(
        /*LocalFrame =*/nullptr,
        blink::WebSize(kTestCanvasCaptureWidth, kTestCanvasCaptureHeight),
        kTestCanvasCaptureFramesPerSecond,
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(), &track_);
  }

  void TearDown() override {
    track_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
    canvas_capture_handler_.reset();

    // Let the message loop run to finish destroying the capturer.
    base::RunLoop().RunUntilIdle();
  }

  // Necessary callbacks and MOCK_METHODS for VideoCapturerSource.
  MOCK_METHOD2(DoOnDeliverFrame,
               void(scoped_refptr<media::VideoFrame>, base::TimeTicks));
  void OnDeliverFrame(scoped_refptr<media::VideoFrame> video_frame,
                      base::TimeTicks estimated_capture_time) {
    DoOnDeliverFrame(std::move(video_frame), estimated_capture_time);
  }

  MOCK_METHOD1(DoOnRunning, void(bool));
  void OnRunning(bool state) { DoOnRunning(state); }

  // Verify returned frames.
  static scoped_refptr<StaticBitmapImage> GenerateTestImage(bool opaque,
                                                            int width,
                                                            int height) {
    SkImageInfo info = SkImageInfo::MakeN32(
        width, height, opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType,
        SkColorSpace::MakeSRGB());
    SkBitmap testBitmap;
    testBitmap.allocPixels(info);
    testBitmap.eraseARGB(opaque ? 255 : kTestAlphaValue, 30, 60, 200);
    return UnacceleratedStaticBitmapImage::Create(
        SkImage::MakeFromBitmap(testBitmap));
  }

  void OnVerifyDeliveredFrame(bool opaque,
                              int expected_width,
                              int expected_height,
                              scoped_refptr<media::VideoFrame> video_frame,
                              base::TimeTicks estimated_capture_time) {
    if (opaque)
      EXPECT_EQ(media::PIXEL_FORMAT_I420, video_frame->format());
    else
      EXPECT_EQ(media::PIXEL_FORMAT_I420A, video_frame->format());

    const gfx::Size& size = video_frame->visible_rect().size();
    EXPECT_EQ(expected_width, size.width());
    EXPECT_EQ(expected_height, size.height());
    const uint8_t* y_plane =
        video_frame->visible_data(media::VideoFrame::kYPlane);
    EXPECT_NEAR(74, y_plane[0], kTestCanvasCaptureFrameColorErrorTolerance);
    const uint8_t* u_plane =
        video_frame->visible_data(media::VideoFrame::kUPlane);
    EXPECT_NEAR(193, u_plane[0], kTestCanvasCaptureFrameColorErrorTolerance);
    const uint8_t* v_plane =
        video_frame->visible_data(media::VideoFrame::kVPlane);
    EXPECT_NEAR(105, v_plane[0], kTestCanvasCaptureFrameColorErrorTolerance);
    if (!opaque) {
      const uint8_t* a_plane =
          video_frame->visible_data(media::VideoFrame::kAPlane);
      EXPECT_EQ(kTestAlphaValue, a_plane[0]);
    }
  }

  blink::WebMediaStreamTrack track_;
  // The Class under test. Needs to be scoped_ptr to force its destruction.
  std::unique_ptr<CanvasCaptureHandler> canvas_capture_handler_;

 protected:
  media::VideoCapturerSource* GetVideoCapturerSource(
      blink::MediaStreamVideoCapturerSource* ms_source) {
    return ms_source->GetSourceForTesting();
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandlerTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, ConstructAndDestruct) {
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  base::RunLoop().RunUntilIdle();
}

// Checks that the destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, DestructTrack) {
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  track_.Reset();
  base::RunLoop().RunUntilIdle();
}

// Checks that the destruction sequence works fine.
TEST_F(CanvasCaptureHandlerTest, DestructHandler) {
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  canvas_capture_handler_.reset();
  base::RunLoop().RunUntilIdle();
}

// Checks that VideoCapturerSource call sequence works fine.
TEST_P(CanvasCaptureHandlerTest, GetFormatsStartAndStop) {
  InSequence s;
  const blink::WebMediaStreamSource& web_media_stream_source = track_.Source();
  EXPECT_FALSE(web_media_stream_source.IsNull());
  blink::MediaStreamVideoCapturerSource* const ms_source =
      static_cast<blink::MediaStreamVideoCapturerSource*>(
          web_media_stream_source.GetPlatformSource());
  EXPECT_TRUE(ms_source);
  media::VideoCapturerSource* source = GetVideoCapturerSource(ms_source);
  EXPECT_TRUE(source);

  media::VideoCaptureFormats formats = source->GetPreferredFormats();
  ASSERT_EQ(2u, formats.size());
  EXPECT_EQ(kTestCanvasCaptureWidth, formats[0].frame_size.width());
  EXPECT_EQ(kTestCanvasCaptureHeight, formats[0].frame_size.height());
  media::VideoCaptureParams params;
  params.requested_format = formats[0];

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
  EXPECT_CALL(*this, DoOnDeliverFrame(_, _))
      .Times(1)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source->StartCapture(
      params,
      base::BindRepeating(&CanvasCaptureHandlerTest::OnDeliverFrame,
                          base::Unretained(this)),
      base::BindRepeating(&CanvasCaptureHandlerTest::OnRunning,
                          base::Unretained(this)));
  canvas_capture_handler_->SendNewFrame(
      GenerateTestImage(testing::get<0>(GetParam()),
                        testing::get<1>(GetParam()),
                        testing::get<2>(GetParam())),
      nullptr);
  run_loop.Run();

  source->StopCapture();
}

// Verifies that SkImage is processed and produces VideoFrame as expected.
TEST_P(CanvasCaptureHandlerTest, VerifyFrame) {
  const bool opaque_frame = testing::get<0>(GetParam());
  const bool width = testing::get<1>(GetParam());
  const bool height = testing::get<1>(GetParam());
  InSequence s;
  media::VideoCapturerSource* const source = GetVideoCapturerSource(
      static_cast<blink::MediaStreamVideoCapturerSource*>(
          track_.Source().GetPlatformSource()));
  EXPECT_TRUE(source);

  base::RunLoop run_loop;
  EXPECT_CALL(*this, DoOnRunning(true)).Times(1);
  media::VideoCaptureParams params;
  source->StartCapture(
      params,
      base::BindRepeating(&CanvasCaptureHandlerTest::OnVerifyDeliveredFrame,
                          base::Unretained(this), opaque_frame, width, height),
      base::BindRepeating(&CanvasCaptureHandlerTest::OnRunning,
                          base::Unretained(this)));
  canvas_capture_handler_->SendNewFrame(
      GenerateTestImage(opaque_frame, width, height), nullptr);
  run_loop.RunUntilIdle();
}

// Checks that needsNewFrame() works as expected.
TEST_F(CanvasCaptureHandlerTest, CheckNeedsNewFrame) {
  InSequence s;
  media::VideoCapturerSource* source = GetVideoCapturerSource(
      static_cast<blink::MediaStreamVideoCapturerSource*>(
          track_.Source().GetPlatformSource()));
  EXPECT_TRUE(source);
  EXPECT_TRUE(canvas_capture_handler_->NeedsNewFrame());
  source->StopCapture();
  EXPECT_FALSE(canvas_capture_handler_->NeedsNewFrame());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CanvasCaptureHandlerTest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(kTestCanvasCaptureFrameEvenSize,
                                         kTestCanvasCaptureFrameOddSize),
                       ::testing::Values(kTestCanvasCaptureFrameEvenSize,
                                         kTestCanvasCaptureFrameOddSize)));

}  // namespace blink
