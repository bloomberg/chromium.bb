// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "media/base/limits.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_filter_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "webkit/media/video_renderer_impl.h"

using media::PipelineStatistics;
using media::PipelineStatus;
using media::VideoDecoder;
using media::VideoFrame;

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace webkit_media {

static const int64 kFrameDurationMs = 10;
static const int kWidth = 320;
static const int kHeight = 240;
static const gfx::Size kNaturalSize(kWidth, kHeight);

class VideoRendererImplTest : public testing::Test {
 public:
  VideoRendererImplTest()
      : timeout_(base::TimeDelta::FromMilliseconds(
            TestTimeouts::action_timeout_ms())),
        cv_(&lock_),
        prerolled_(false) {
    // Bind callbacks with various values for TestPainting() tests.
    natural_frame_cb_ = base::Bind(
        &VideoRendererImplTest::DeliverFrame, base::Unretained(this),
        kWidth, kHeight);
    larger_frame_cb_ = base::Bind(
        &VideoRendererImplTest::DeliverFrame, base::Unretained(this),
        kWidth * 2, kHeight * 2);
    smaller_frame_cb_ = base::Bind(
        &VideoRendererImplTest::DeliverFrame, base::Unretained(this),
        kWidth / 2, kHeight / 2);
    fast_paint_cb_ = base::Bind(
        &VideoRendererImplTest::Paint, base::Unretained(this), true);
    slow_paint_cb_ = base::Bind(
        &VideoRendererImplTest::Paint, base::Unretained(this), false);

    // Forward all time requests to lock-protected getter.
    ON_CALL(host_, GetTime())
        .WillByDefault(Invoke(this, &VideoRendererImplTest::GetTime));
    ON_CALL(host_, GetDuration())
        .WillByDefault(Return(base::TimeDelta::FromSeconds(10)));

    decoder_ = new media::MockVideoDecoder();
    EXPECT_CALL(*decoder_, natural_size())
        .WillOnce(ReturnRef(kNaturalSize));

    // Initialize the renderer.
    base::WaitableEvent event(false, false);
    renderer_ = new VideoRendererImpl(
        base::Bind(
            &VideoRendererImplTest::OnPaint, base::Unretained(this)),
        base::Bind(
            &VideoRendererImplTest::SetOpaque, base::Unretained(this)));
    renderer_->set_host(&host_);
    renderer_->SetPlaybackRate(1.0f);
    renderer_->Initialize(
        decoder_,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)),
        base::Bind(&VideoRendererImplTest::OnStatistics,
                   base::Unretained(this)));
    CHECK(event.TimedWait(timeout_)) << "Timed out waiting to initialize.";
  }

  virtual ~VideoRendererImplTest() {
    // Stop the renderer.
    base::WaitableEvent event(false, false);
    renderer_->Stop(
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));
    CHECK(event.TimedWait(timeout_)) << "Timed out waiting to stop.";
  }

  typedef base::Callback<void(int64)> DeliverCB;
  void TestPainting(const DeliverCB& deliver_first_frame,
                    const DeliverCB& deliver_remaining_frames,
                    const base::Closure& paint) {
    StartPrerolling();
    for (int i = 0; i < media::limits::kMaxVideoFrames; ++i) {
      WaitForRead();
      if (i == 0)
        deliver_first_frame.Run(i * kFrameDurationMs);
      else
        deliver_remaining_frames.Run(i * kFrameDurationMs);
    }

    // Wait for prerolling to complete and paint first frame.
    WaitForPrerolled();
    paint.Run();

    // Advance to remaining frames and paint again.
    Play();
    for (int i = 1; i < media::limits::kMaxVideoFrames; ++i) {
      SetTime(i * kFrameDurationMs);
      paint.Run();
    }
    WaitForRead();
    paint.Run();
  }

  const DeliverCB& natural_frame_cb() { return natural_frame_cb_; }
  const DeliverCB& larger_frame_cb() { return larger_frame_cb_; }
  const DeliverCB& smaller_frame_cb() { return smaller_frame_cb_; }
  const base::Closure& fast_paint_cb() { return fast_paint_cb_; }
  const base::Closure& slow_paint_cb() { return slow_paint_cb_; }

 private:
  void OnPaint() {}
  void SetOpaque(bool) {}
  void OnStatistics(const PipelineStatistics&) {}

  void StartPrerolling() {
    EXPECT_CALL(*decoder_, Read(_))
        .WillRepeatedly(Invoke(this, &VideoRendererImplTest::FrameRequested));

    renderer_->Seek(base::TimeDelta::FromMilliseconds(0),
                    base::Bind(&VideoRendererImplTest::PrerollDone,
                               base::Unretained(this),
                               media::PIPELINE_OK));
  }

  void Play() {
    base::WaitableEvent event(false, false);
    renderer_->Play(
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));
    CHECK(event.TimedWait(timeout_)) << "Timed out waiting to play.";
  }

  void SetTime(int64 timestamp) {
    base::AutoLock l(lock_);
    time_ = base::TimeDelta::FromMilliseconds(timestamp);
  }

  base::TimeDelta GetTime() {
    base::AutoLock l(lock_);
    return time_;
  }

  void WaitForRead() {
    base::AutoLock l(lock_);
    if (!read_cb_.is_null())
      return;
    cv_.TimedWait(timeout_);
    CHECK(!read_cb_.is_null()) << "Timed out waiting for read.";
  }

  void WaitForPrerolled() {
    base::AutoLock l(lock_);
    if (prerolled_)
      return;
    cv_.TimedWait(timeout_);
    CHECK(prerolled_) << "Timed out waiting for prerolled.";
  }

  void FrameRequested(const VideoDecoder::ReadCB& callback) {
    base::AutoLock l(lock_);
    CHECK(read_cb_.is_null());
    read_cb_ = callback;
    cv_.Signal();
  }

  void PrerollDone(media::PipelineStatus expected_status,
                   PipelineStatus status) {
    base::AutoLock l(lock_);
    EXPECT_EQ(status, expected_status);
    prerolled_ = true;
    cv_.Signal();
  }

  // Allocates a frame of |width| and |height| dimensions with |timestamp|
  // and delivers it to the renderer by running |read_cb_|.
  void DeliverFrame(int width, int height, int64 timestamp) {
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::CreateBlackFrame(width, height);
    video_frame->SetTimestamp(base::TimeDelta::FromMilliseconds(timestamp));
    video_frame->SetDuration(
        base::TimeDelta::FromMilliseconds(kFrameDurationMs));

    VideoDecoder::ReadCB read_cb;
    {
      base::AutoLock l(lock_);
      CHECK(!read_cb_.is_null());
      std::swap(read_cb_, read_cb);
    }

    read_cb.Run(video_frame);
  }

  // Triggers the fast painting path when |opaque| is true and the slow
  // painting path when |opaque| is false.
  void Paint(bool opaque) {
    SkDevice device(SkBitmap::kARGB_8888_Config, kWidth, kHeight, opaque);
    SkCanvas canvas(&device);
    gfx::Rect rect(0, 0, kWidth, kHeight);
    renderer_->Paint(&canvas, rect);
  }

  NiceMock<media::MockFilterHost> host_;
  scoped_refptr<VideoRendererImpl> renderer_;
  scoped_refptr<media::MockVideoDecoder> decoder_;

  base::TimeDelta timeout_;

  base::Lock lock_;
  base::ConditionVariable cv_;

  // Protected by |lock_|.
  base::TimeDelta time_;
  VideoDecoder::ReadCB read_cb_;
  bool prerolled_;

  // Prebound closures for use with TestPainting().
  DeliverCB natural_frame_cb_;
  DeliverCB larger_frame_cb_;
  DeliverCB smaller_frame_cb_;
  base::Closure slow_paint_cb_;
  base::Closure fast_paint_cb_;

  DISALLOW_COPY_AND_ASSIGN(VideoRendererImplTest);
};

TEST_F(VideoRendererImplTest, FastPaint_Natural) {
  TestPainting(natural_frame_cb(), natural_frame_cb(), fast_paint_cb());
}

TEST_F(VideoRendererImplTest, SlowPaint_Natural) {
  TestPainting(natural_frame_cb(), natural_frame_cb(), slow_paint_cb());
}

TEST_F(VideoRendererImplTest, FastPaint_Larger) {
  TestPainting(natural_frame_cb(), larger_frame_cb(), fast_paint_cb());
}

TEST_F(VideoRendererImplTest, SlowPaint_Larger) {
  TestPainting(natural_frame_cb(), larger_frame_cb(), slow_paint_cb());
}

TEST_F(VideoRendererImplTest, FastPaint_Smaller) {
  TestPainting(natural_frame_cb(), smaller_frame_cb(), fast_paint_cb());
}

TEST_F(VideoRendererImplTest, SlowPaint_Smaller) {
  TestPainting(natural_frame_cb(), smaller_frame_cb(), slow_paint_cb());
}

}  // namespace webkit_media
