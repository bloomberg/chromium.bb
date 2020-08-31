// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_renderer_sink.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Mock;

namespace blink {

class MediaStreamVideoRendererSinkTest : public testing::Test {
 public:
  MediaStreamVideoRendererSinkTest()
      : mock_source_(new MockMediaStreamVideoSource()) {
    blink_source_.Initialize(WebString::FromASCII("dummy_source_id"),
                             WebMediaStreamSource::kTypeVideo,
                             WebString::FromASCII("dummy_source_name"),
                             false /* remote */);
    blink_source_.SetPlatformSource(base::WrapUnique(mock_source_));
    blink_track_ = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true);
    mock_source_->StartMockedSource();
    base::RunLoop().RunUntilIdle();

    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        blink_track_,
        ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
            &MediaStreamVideoRendererSinkTest::RepaintCallback,
            CrossThreadUnretained(this))),
        Platform::Current()->GetIOTaskRunner(),
        scheduler::GetSingleThreadTaskRunnerForTesting());
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(IsInStoppedState());
  }

  void TearDown() override {
    media_stream_video_renderer_sink_ = nullptr;
    blink_source_.Reset();
    blink_track_.Reset();
    WebHeap::CollectAllGarbageForTesting();

    // Let the message loop run to finish destroying the pool.
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(RepaintCallback, void(scoped_refptr<media::VideoFrame>));

  bool IsInStartedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::STARTED;
  }
  bool IsInStoppedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::STOPPED;
  }
  bool IsInPausedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::PAUSED;
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame) {
    mock_source_->DeliverVideoFrame(frame);
    base::RunLoop().RunUntilIdle();

    RunIOUntilIdle();
  }

  scoped_refptr<MediaStreamVideoRendererSink> media_stream_video_renderer_sink_;

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  WebMediaStreamTrack blink_track_;

 private:
  void RunIOUntilIdle() const {
    // |blink_track_| uses IO thread to send frames to sinks. Make sure that
    // tasks on IO thread are completed before moving on.
    base::RunLoop run_loop;
    Platform::Current()->GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

  WebMediaStreamSource blink_source_;
  MockMediaStreamVideoSource* mock_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoRendererSinkTest);
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(MediaStreamVideoRendererSinkTest, StartStop) {
  EXPECT_TRUE(IsInStoppedState());

  media_stream_video_renderer_sink_->Start();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Pause();
  EXPECT_TRUE(IsInPausedState());

  media_stream_video_renderer_sink_->Resume();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Stop();
  EXPECT_TRUE(IsInStoppedState());
}

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_F(MediaStreamVideoRendererSinkTest, EncodeVideoFrames) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  OnVideoFrame(video_frame);

  media_stream_video_renderer_sink_->Stop();
}

class MediaStreamVideoRendererSinkTransparencyTest
    : public MediaStreamVideoRendererSinkTest {
 public:
  MediaStreamVideoRendererSinkTransparencyTest() {
    media_stream_video_renderer_sink_ = new MediaStreamVideoRendererSink(
        blink_track_,
        ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
            &MediaStreamVideoRendererSinkTransparencyTest::
                VerifyTransparentFrame,
            CrossThreadUnretained(this))),
        Platform::Current()->GetIOTaskRunner(),
        scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void VerifyTransparentFrame(scoped_refptr<media::VideoFrame> frame) {
    EXPECT_EQ(media::PIXEL_FORMAT_I420A, frame->format());
  }
};

TEST_F(MediaStreamVideoRendererSinkTransparencyTest, SendTransparentFrame) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const gfx::Size kSize(10, 10);
  const base::TimeDelta kTimestamp = base::TimeDelta();
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420A, kSize,
                                     gfx::Rect(kSize), kSize, kTimestamp);
  OnVideoFrame(video_frame);
  base::RunLoop().RunUntilIdle();

  media_stream_video_renderer_sink_->Stop();
}

}  // namespace blink
