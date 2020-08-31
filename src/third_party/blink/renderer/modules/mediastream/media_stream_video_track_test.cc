// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// To avoid symbol collisions in jumbo builds.
namespace media_stream_video_track_test {

using base::test::RunOnceClosure;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Values;

using ContentHintType = WebMediaStreamTrack::ContentHintType;

const uint8_t kBlackValue = 0x00;
const uint8_t kColorValue = 0xAB;
const int kMockSourceWidth = 640;
const int kMockSourceHeight = 480;

class MediaStreamVideoTrackTest
    : public testing::TestWithParam<ContentHintType> {
 public:
  MediaStreamVideoTrackTest() : mock_source_(nullptr), source_started_(false) {}

  ~MediaStreamVideoTrackTest() override {}

  void TearDown() override {
    blink_source_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  void DeliverVideoFrameAndWaitForRenderer(
      scoped_refptr<media::VideoFrame> frame,
      MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnVideoFrame)
        .WillOnce(RunOnceClosure(std::move(quit_closure)));
    mock_source()->DeliverVideoFrame(std::move(frame));
    run_loop.Run();
  }

  void DeliverEncodedVideoFrameAndWait(scoped_refptr<EncodedVideoFrame> frame,
                                       MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnEncodedVideoFrame)
        .WillOnce(
            Invoke([&](base::TimeTicks) { std::move(quit_closure).Run(); }));
    mock_source()->DeliverEncodedVideoFrame(frame);
    run_loop.Run();
  }

  void DeliverDefaultSizeVideoFrameAndWaitForRenderer(
      MockMediaStreamVideoSink* sink) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            kColorValue, kColorValue, kColorValue, base::TimeDelta());
    DeliverVideoFrameAndWaitForRenderer(frame, sink);
  }

 protected:
  virtual void InitializeSource() {
    blink_source_.Reset();
    mock_source_ = new MockMediaStreamVideoSource(
        media::VideoCaptureFormat(
            gfx::Size(kMockSourceWidth, kMockSourceHeight), 30.0,
            media::PIXEL_FORMAT_I420),
        false);
    blink_source_.Initialize(WebString::FromASCII("dummy_source_id"),
                             WebMediaStreamSource::kTypeVideo,
                             WebString::FromASCII("dummy_source_name"),
                             false /* remote */);
    blink_source_.SetPlatformSource(base::WrapUnique(mock_source_));
  }

  // Create a track that's associated with |mock_source_|.
  WebMediaStreamTrack CreateTrack() {
    const bool enabled = true;
    WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  // Create a track that's associated with |mock_source_| and has the given
  // |adapter_settings|.
  WebMediaStreamTrack CreateTrackWithSettings(
      const VideoTrackAdapterSettings& adapter_settings) {
    const bool enabled = true;
    WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, adapter_settings, base::Optional<bool>(), false, 0.0,
        WebPlatformMediaStreamSource::ConstraintsOnceCallback(), enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  void UpdateVideoSourceToRespondToRequestRefreshFrame() {
    blink_source_.Reset();
    mock_source_ = new MockMediaStreamVideoSource(
        media::VideoCaptureFormat(
            gfx::Size(kMockSourceWidth, kMockSourceHeight), 30.0,
            media::PIXEL_FORMAT_I420),
        true);
    blink_source_.Initialize(WebString::FromASCII("dummy_source_id"),
                             WebMediaStreamSource::kTypeVideo,
                             WebString::FromASCII("dummy_source_name"),
                             false /* remote */);
    blink_source_.SetPlatformSource(base::WrapUnique(mock_source_));
  }

  void DepleteIOCallbacks() {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    mock_source()->io_task_runner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([&] { std::move(quit_closure).Run(); }));
    run_loop.Run();
  }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }
  const WebMediaStreamSource& blink_source() const { return blink_source_; }

 private:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  WebMediaStreamSource blink_source_;
  // |mock_source_| is owned by |webkit_source_|.
  MockMediaStreamVideoSource* mock_source_;
  bool source_started_;
};

TEST_F(MediaStreamVideoTrackTest, AddAndRemoveSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);

  sink.DisconnectFromTrack();

  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                MediaStreamVideoSource::kDefaultHeight));
  mock_source()->DeliverVideoFrame(frame);
  // Wait for the IO thread to complete delivering frames.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, sink.number_of_frames());
}

class CheckThreadHelper {
 public:
  CheckThreadHelper(base::OnceClosure callback, bool* correct)
      : callback_(std::move(callback)), correct_(correct) {}

  ~CheckThreadHelper() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    *correct_ = true;
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
  bool* correct_;
  THREAD_CHECKER(thread_checker_);
};

void CheckThreadVideoFrameReceiver(CheckThreadHelper* helper,
                                   scoped_refptr<media::VideoFrame> frame,
                                   base::TimeTicks estimated_capture_time) {
  // Do nothing.
}

// Checks that the callback given to the track is reset on the right thread.
TEST_F(MediaStreamVideoTrackTest, ResetCallbackOnThread) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();

  base::RunLoop run_loop;
  bool correct = false;
  sink.ConnectToTrackWithCallback(
      track, WTF::BindRepeating(&CheckThreadVideoFrameReceiver,
                                base::Owned(new CheckThreadHelper(
                                    run_loop.QuitClosure(), &correct))));
  sink.DisconnectFromTrack();
  run_loop.Run();
  EXPECT_TRUE(correct) << "Not called on correct thread.";
}

TEST_F(MediaStreamVideoTrackTest, SetEnabled) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);

  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());
  EXPECT_EQ(kColorValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));

  video_track->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(2, sink.number_of_frames());
  EXPECT_EQ(kBlackValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));

  video_track->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(3, sink.number_of_frames());
  EXPECT_EQ(kColorValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));
  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, SourceDetached) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MockMediaStreamVideoSink sink;
  auto* video_track = MediaStreamVideoTrack::GetVideoTrack(track);
  video_track->StopAndNotify(base::BindOnce([] {}));
  sink.ConnectToTrack(track);
  sink.ConnectEncodedToTrack(track);
  video_track->SetEnabled(true);
  video_track->SetEnabled(false);
  WebMediaStreamTrack::Settings settings;
  video_track->GetSettings(settings);
  sink.DisconnectFromTrack();
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, SourceStopped) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink.state());
  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, StopLastTrack) {
  InitializeSource();
  MockMediaStreamVideoSink sink1;
  WebMediaStreamTrack track1 = CreateTrack();
  sink1.ConnectToTrack(track1);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink1.state());

  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            blink_source().GetReadyState());

  MockMediaStreamVideoSink sink2;
  WebMediaStreamTrack track2 = CreateTrack();
  sink2.ConnectToTrack(track2);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink2.state());

  MediaStreamVideoTrack* const native_track1 =
      MediaStreamVideoTrack::GetVideoTrack(track1);
  native_track1->Stop();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink1.state());
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            blink_source().GetReadyState());
  sink1.DisconnectFromTrack();

  MediaStreamVideoTrack* const native_track2 =
      MediaStreamVideoTrack::GetVideoTrack(track2);
  native_track2->Stop();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink2.state());
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded,
            blink_source().GetReadyState());
  sink2.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, CheckTrackRequestsFrame) {
  InitializeSource();
  UpdateVideoSourceToRespondToRequestRefreshFrame();
  WebMediaStreamTrack track = CreateTrack();

  // Add sink and expect to get a frame.
  MockMediaStreamVideoSink sink;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  sink.ConnectToTrack(track);
  run_loop.Run();
  EXPECT_EQ(1, sink.number_of_frames());

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, GetSettings) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  // These values come straight from the mock video track implementation.
  EXPECT_EQ(640, settings.width);
  EXPECT_EQ(480, settings.height);
  EXPECT_EQ(30.0, settings.frame_rate);
  EXPECT_EQ(WebMediaStreamTrack::FacingMode::kNone, settings.facing_mode);
}

TEST_F(MediaStreamVideoTrackTest, GetSettingsWithAdjustment) {
  InitializeSource();
  const int kAdjustedWidth = 600;
  const int kAdjustedHeight = 400;
  const double kAdjustedFrameRate = 20.0;
  VideoTrackAdapterSettings adapter_settings(
      gfx::Size(kAdjustedWidth, kAdjustedHeight), 0.0, 10000.0,
      kAdjustedFrameRate);
  WebMediaStreamTrack track = CreateTrackWithSettings(adapter_settings);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(kAdjustedWidth, settings.width);
  EXPECT_EQ(kAdjustedHeight, settings.height);
  EXPECT_EQ(kAdjustedFrameRate, settings.frame_rate);
  EXPECT_EQ(WebMediaStreamTrack::FacingMode::kNone, settings.facing_mode);
}

TEST_F(MediaStreamVideoTrackTest, GetSettingsStopped) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  native_track->Stop();
  WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(-1, settings.width);
  EXPECT_EQ(-1, settings.height);
  EXPECT_EQ(-1, settings.frame_rate);
  EXPECT_EQ(WebMediaStreamTrack::FacingMode::kNone, settings.facing_mode);
  EXPECT_TRUE(settings.device_id.IsNull());
}

TEST_F(MediaStreamVideoTrackTest, DeliverFramesAndGetSettings) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  WebMediaStreamTrack::Settings settings;

  auto frame1 = media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400));
  DeliverVideoFrameAndWaitForRenderer(std::move(frame1), &sink);
  native_track->GetSettings(settings);
  EXPECT_EQ(600, settings.width);
  EXPECT_EQ(400, settings.height);

  auto frame2 = media::VideoFrame::CreateBlackFrame(gfx::Size(200, 300));
  DeliverVideoFrameAndWaitForRenderer(std::move(frame2), &sink);
  native_track->GetSettings(settings);
  EXPECT_EQ(200, settings.width);
  EXPECT_EQ(300, settings.height);

  sink.DisconnectFromTrack();
}

TEST_P(MediaStreamVideoTrackTest, PropagatesContentHintType) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  MediaStreamVideoTrack::GetVideoTrack(track)->SetContentHint(GetParam());
  EXPECT_EQ(sink.content_hint(), GetParam());
  sink.DisconnectFromTrack();
}

class MediaStreamVideoTrackEncodedTest : public MediaStreamVideoTrackTest {
 public:
  void InitializeSource() override {
    MediaStreamVideoTrackTest::InitializeSource();
    ON_CALL(*mock_source(), SupportsEncodedOutput).WillByDefault(Return(true));
  }
};

TEST_F(MediaStreamVideoTrackEncodedTest, ConnectEncodedSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  EXPECT_CALL(*mock_source(), OnEncodedSinkEnabled);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true)).Times(0);
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled).Times(0);
  sink.ConnectEncodedToTrack(track);
  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, DisconnectEncodedSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest,
       CapturingLinkSecuredWithEncodedAndNormalTracks) {
  InitializeSource();
  MockMediaStreamVideoSink sink1;
  WebMediaStreamTrack track1 = CreateTrack();
  InSequence s;
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink1.ConnectToTrack(track1);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  MockMediaStreamVideoSink sink2;
  WebMediaStreamTrack track2 = CreateTrack();
  sink2.ConnectEncodedToTrack(track2);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  sink1.DisconnectFromTrack();
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink2.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, TransferOneEncodedVideoFrame) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnEncodedVideoFrame).WillOnce(Invoke([&](base::TimeTicks) {
    std::move(quit_closure).Run();
  }));
  mock_source()->DeliverEncodedVideoFrame(
      base::MakeRefCounted<MockEncodedVideoFrame>());
  run_loop.Run();
  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, SupportsEncodedDisableEnable) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);

  auto key_frame = base::MakeRefCounted<MockEncodedVideoFrame>();
  EXPECT_CALL(*key_frame, IsKeyFrame).WillRepeatedly(Return(true));
  auto delta_frame = base::MakeRefCounted<MockEncodedVideoFrame>();
  EXPECT_CALL(*delta_frame, IsKeyFrame).WillRepeatedly(Return(false));

  DeliverEncodedVideoFrameAndWait(key_frame, &sink);

  // Key frame when disabled -> shouldn't get dispatched
  MediaStreamVideoTrack::GetVideoTrack(track)->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());
  {
    EXPECT_CALL(sink, OnEncodedVideoFrame).Times(0);
    mock_source()->DeliverEncodedVideoFrame(key_frame);
    DepleteIOCallbacks();
  }

  // Delta frame when disabled -> shouldn't get dispatched until key frame
  // appears.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame);
  MediaStreamVideoTrack::GetVideoTrack(track)->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  {
    EXPECT_CALL(sink, OnEncodedVideoFrame).Times(0);
    mock_source()->DeliverEncodedVideoFrame(delta_frame);
    DepleteIOCallbacks();
  }

  // After a key frame things should be flowing again.
  DeliverEncodedVideoFrameAndWait(key_frame, &sink);
  DeliverEncodedVideoFrameAndWait(delta_frame, &sink);

  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_P(MediaStreamVideoTrackEncodedTest, PropagatesContentHintType) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  MediaStreamVideoTrack::GetVideoTrack(track)->SetContentHint(GetParam());
  EXPECT_EQ(sink.content_hint(), GetParam());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, SourceStopped) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink.state());
  sink.DisconnectEncodedFromTrack();
}

INSTANTIATE_TEST_SUITE_P(,
                         MediaStreamVideoTrackTest,
                         Values(ContentHintType::kVideoMotion,
                                ContentHintType::kVideoDetail,
                                ContentHintType::kVideoText));

INSTANTIATE_TEST_SUITE_P(,
                         MediaStreamVideoTrackEncodedTest,
                         Values(ContentHintType::kVideoMotion,
                                ContentHintType::kVideoDetail,
                                ContentHintType::kVideoText));

}  // namespace media_stream_video_track_test
}  // namespace blink
