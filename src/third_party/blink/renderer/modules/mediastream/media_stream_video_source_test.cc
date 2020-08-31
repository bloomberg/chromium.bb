// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_constraint_factory.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

namespace blink {

class MediaStreamVideoSourceTest : public testing::Test {
 public:
  MediaStreamVideoSourceTest()
      : number_of_successful_constraints_applied_(0),
        number_of_failed_constraints_applied_(0),
        result_(mojom::MediaStreamRequestResult::OK),
        result_name_(""),
        mock_source_(new MockMediaStreamVideoSource(
            media::VideoCaptureFormat(gfx::Size(1280, 720),
                                      1000.0,
                                      media::PIXEL_FORMAT_I420),
            false)) {
    mock_source_->DisableStopForRestart();
    media::VideoCaptureFormats formats;
    formats.push_back(media::VideoCaptureFormat(gfx::Size(1280, 720), 30,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(640, 480), 30,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(352, 288), 30,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(320, 240), 30,
                                                media::PIXEL_FORMAT_I420));
    web_source_.Initialize(WebString::FromASCII("dummy_source_id"),
                           WebMediaStreamSource::kTypeVideo,
                           WebString::FromASCII("dummy_source_name"),
                           false /* remote */);
    web_source_.SetPlatformSource(base::WrapUnique(mock_source_));
    ON_CALL(*mock_source_, SupportsEncodedOutput).WillByDefault(Return(true));
  }

  void TearDown() override {
    web_source_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  MOCK_METHOD0(MockNotification, void());

 protected:
  MediaStreamVideoSource* source() { return mock_source_; }

  // Create a track that's associated with |web_source_|.
  WebMediaStreamTrack CreateTrack(const std::string& id) {
    bool enabled = true;
    return MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_,
        WTF::Bind(&MediaStreamVideoSourceTest::OnConstraintsApplied,
                  base::Unretained(this)),
        enabled);
  }

  WebMediaStreamTrack CreateTrack(
      const std::string& id,
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screencast,
      double min_frame_rate) {
    bool enabled = true;
    return MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, adapter_settings, noise_reduction, is_screencast,
        min_frame_rate,
        WTF::Bind(&MediaStreamVideoSourceTest::OnConstraintsApplied,
                  base::Unretained(this)),
        enabled);
  }

  WebMediaStreamTrack CreateTrack() {
    return CreateTrack("123",
                       VideoTrackAdapterSettings(gfx::Size(100, 100), 30.0),
                       base::Optional<bool>(), false, 0.0);
  }

  WebMediaStreamTrack CreateTrackAndStartSource(int width,
                                                int height,
                                                double frame_rate,
                                                bool detect_rotation = false) {
    WebMediaStreamTrack track = CreateTrack(
        "123", VideoTrackAdapterSettings(gfx::Size(width, height), frame_rate),
        base::Optional<bool>(), false, 0.0);

    EXPECT_EQ(0, NumberOfSuccessConstraintsCallbacks());
    mock_source_->StartMockedSource();
    // Once the source has started successfully we expect that the
    // ConstraintsOnceCallback in WebPlatformMediaStreamSource::AddTrack
    // completes.
    EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
    return track;
  }

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_constraints_applied_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_constraints_applied_;
  }

  mojom::MediaStreamRequestResult error_type() const { return result_; }
  WebString error_name() const { return result_name_; }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }

  const WebMediaStreamSource& web_source() { return web_source_; }

  void TestSourceCropFrame(int capture_width,
                           int capture_height,
                           int expected_width,
                           int expected_height) {
    // Configure the track to crop to the expected resolution.
    WebMediaStreamTrack track =
        CreateTrackAndStartSource(expected_width, expected_height, 30.0);

    // Produce frames at the capture resolution.
    MockMediaStreamVideoSink sink;
    sink.ConnectToTrack(track);
    DeliverVideoFrameAndWaitForRenderer(capture_width, capture_height, &sink);
    EXPECT_EQ(1, sink.number_of_frames());

    // Expect the delivered frame to be cropped.
    EXPECT_EQ(expected_height, sink.frame_size().height());
    EXPECT_EQ(expected_width, sink.frame_size().width());
    sink.DisconnectFromTrack();
  }

  void DeliverVideoFrameAndWaitForRenderer(int width,
                                           int height,
                                           MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnVideoFrame).WillOnce([&](base::TimeTicks) {
      std::move(quit_closure).Run();
    });
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(width, height));
    mock_source()->DeliverVideoFrame(frame);
    run_loop.Run();
  }

  void DeliverRotatedVideoFrameAndWaitForRenderer(
      int width,
      int height,
      MockMediaStreamVideoSink* sink) {
    DeliverVideoFrameAndWaitForRenderer(height, width, sink);
  }

  void DeliverVideoFrameAndWaitForTwoRenderers(
      int width,
      int height,
      MockMediaStreamVideoSink* sink1,
      MockMediaStreamVideoSink* sink2) {
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink1, OnVideoFrame);
    EXPECT_CALL(*sink2, OnVideoFrame).WillOnce([&](base::TimeTicks) {
      std::move(quit_closure).Run();
    });
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(width, height));
    mock_source()->DeliverVideoFrame(frame);
    run_loop.Run();
  }

  void TestTwoTracksWithDifferentSettings(int capture_width,
                                          int capture_height,
                                          int expected_width1,
                                          int expected_height1,
                                          int expected_width2,
                                          int expected_height2) {
    WebMediaStreamTrack track1 =
        CreateTrackAndStartSource(expected_width1, expected_height1,
                                  MediaStreamVideoSource::kDefaultFrameRate);

    WebMediaStreamTrack track2 = CreateTrack(
        "dummy",
        VideoTrackAdapterSettings(gfx::Size(expected_width2, expected_height2),
                                  MediaStreamVideoSource::kDefaultFrameRate),
        base::Optional<bool>(), false, 0.0);

    MockMediaStreamVideoSink sink1;
    sink1.ConnectToTrack(track1);
    EXPECT_EQ(0, sink1.number_of_frames());

    MockMediaStreamVideoSink sink2;
    sink2.ConnectToTrack(track2);
    EXPECT_EQ(0, sink2.number_of_frames());

    DeliverVideoFrameAndWaitForTwoRenderers(capture_width, capture_height,
                                            &sink1, &sink2);

    EXPECT_EQ(1, sink1.number_of_frames());
    EXPECT_EQ(expected_width1, sink1.frame_size().width());
    EXPECT_EQ(expected_height1, sink1.frame_size().height());

    EXPECT_EQ(1, sink2.number_of_frames());
    EXPECT_EQ(expected_width2, sink2.frame_size().width());
    EXPECT_EQ(expected_height2, sink2.frame_size().height());

    sink1.DisconnectFromTrack();
    sink2.DisconnectFromTrack();
  }

  void ReleaseTrackAndSourceOnAddTrackCallback(
      const WebMediaStreamTrack& track_to_release) {
    track_to_release_ = track_to_release;
  }

 private:
  void OnConstraintsApplied(WebPlatformMediaStreamSource* source,
                            mojom::MediaStreamRequestResult result,
                            const WebString& result_name) {
    ASSERT_EQ(source, web_source().GetPlatformSource());

    if (result == mojom::MediaStreamRequestResult::OK) {
      ++number_of_successful_constraints_applied_;
    } else {
      result_ = result;
      result_name_ = result_name;
      ++number_of_failed_constraints_applied_;
    }

    if (!track_to_release_.IsNull()) {
      mock_source_ = nullptr;
      web_source_.Reset();
      track_to_release_.Reset();
    }
  }
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  WebMediaStreamTrack track_to_release_;
  int number_of_successful_constraints_applied_;
  int number_of_failed_constraints_applied_;
  mojom::MediaStreamRequestResult result_;
  WebString result_name_;
  WebMediaStreamSource web_source_;
  // |mock_source_| is owned by |web_source_|.
  MockMediaStreamVideoSource* mock_source_;
};

TEST_F(MediaStreamVideoSourceTest, AddTrackAndStartSource) {
  WebMediaStreamTrack track = CreateTrack("123");
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTwoTracksBeforeSourceStarts) {
  WebMediaStreamTrack track1 = CreateTrack("123");
  WebMediaStreamTrack track2 = CreateTrack("123");
  EXPECT_EQ(0, NumberOfSuccessConstraintsCallbacks());
  mock_source()->StartMockedSource();
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAfterSourceStarts) {
  WebMediaStreamTrack track1 = CreateTrack("123");
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  WebMediaStreamTrack track2 = CreateTrack("123");
  EXPECT_EQ(2, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAndFailToStartSource) {
  WebMediaStreamTrack track = CreateTrack("123");
  mock_source()->FailToStartMockedSource();
  EXPECT_EQ(1, NumberOfFailedConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, MandatoryAspectRatio4To3) {
  TestSourceCropFrame(1280, 720, 960, 720);
}

TEST_F(MediaStreamVideoSourceTest, ReleaseTrackAndSourceOnSuccessCallBack) {
  WebMediaStreamTrack track = CreateTrack("123");
  ReleaseTrackAndSourceOnAddTrackCallback(track);
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, TwoTracksWithVGAAndWVGA) {
  TestTwoTracksWithDifferentSettings(640, 480, 640, 480, 640, 360);
}

TEST_F(MediaStreamVideoSourceTest, TwoTracksWith720AndWVGA) {
  TestTwoTracksWithDifferentSettings(1280, 720, 1280, 720, 640, 360);
}

TEST_F(MediaStreamVideoSourceTest, SourceChangeFrameSize) {
  // Expect the source to start capture with the supported resolution.
  // Disable frame-rate adjustment in spec-compliant mode to ensure no frames
  // are dropped.
  WebMediaStreamTrack track = CreateTrackAndStartSource(800, 700, 0.0);

  MockMediaStreamVideoSink sink;
  sink.ConnectToTrack(track);
  EXPECT_EQ(0, sink.number_of_frames());
  DeliverVideoFrameAndWaitForRenderer(320, 240, &sink);
  EXPECT_EQ(1, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since its smaller than
  // max requested.
  EXPECT_EQ(320, sink.frame_size().width());
  EXPECT_EQ(240, sink.frame_size().height());

  DeliverVideoFrameAndWaitForRenderer(640, 480, &sink);
  EXPECT_EQ(2, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since its smaller than
  // max requested.
  EXPECT_EQ(640, sink.frame_size().width());
  EXPECT_EQ(480, sink.frame_size().height());

  DeliverVideoFrameAndWaitForRenderer(1280, 720, &sink);
  EXPECT_EQ(3, sink.number_of_frames());
  // Expect a frame to be cropped since its larger than max requested.
  EXPECT_EQ(800, sink.frame_size().width());
  EXPECT_EQ(700, sink.frame_size().height());

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoSourceTest, RotatedSourceDetectionDisabled) {
  source()->SetDeviceRotationDetection(false /* enabled */);

  // Expect the source to start capture with the supported resolution.
  // Disable frame-rate adjustment in spec-compliant mode to ensure no frames
  // are dropped.
  WebMediaStreamTrack track = CreateTrackAndStartSource(1280, 720, 0.0, true);

  MockMediaStreamVideoSink sink;
  sink.ConnectToTrack(track);
  EXPECT_EQ(0, sink.number_of_frames());
  DeliverVideoFrameAndWaitForRenderer(1280, 720, &sink);
  EXPECT_EQ(1, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since it is the same size
  // as the source native format.
  EXPECT_EQ(1280, sink.frame_size().width());
  EXPECT_EQ(720, sink.frame_size().height());

  DeliverRotatedVideoFrameAndWaitForRenderer(1280, 720, &sink);
  EXPECT_EQ(2, sink.number_of_frames());
  // Expect the delivered frame to be cropped because the rotation is not
  // detected.
  EXPECT_EQ(720, sink.frame_size().width());
  EXPECT_EQ(720, sink.frame_size().height());

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoSourceTest, RotatedSourceDetectionEnabled) {
  source()->SetDeviceRotationDetection(true /* enabled */);

  // Expect the source to start capture with the supported resolution.
  // Disable frame-rate adjustment in spec-compliant mode to ensure no frames
  // are dropped.
  WebMediaStreamTrack track = CreateTrackAndStartSource(1280, 720, 0.0, true);

  MockMediaStreamVideoSink sink;
  sink.ConnectToTrack(track);
  EXPECT_EQ(0, sink.number_of_frames());
  DeliverVideoFrameAndWaitForRenderer(1280, 720, &sink);
  EXPECT_EQ(1, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since it is the same size
  // as the source native format.
  EXPECT_EQ(1280, sink.frame_size().width());
  EXPECT_EQ(720, sink.frame_size().height());

  DeliverRotatedVideoFrameAndWaitForRenderer(1280, 720, &sink);
  EXPECT_EQ(2, sink.number_of_frames());
  // Expect the delivered frame to be passed unchanged since it is detected as
  // a valid frame on a rotated device.
  EXPECT_EQ(720, sink.frame_size().width());
  EXPECT_EQ(1280, sink.frame_size().height());

  sink.DisconnectFromTrack();
}

// Test that a source producing no frames change the source ReadyState to muted.
// that in a reasonable time frame the muted state turns to false.
TEST_F(MediaStreamVideoSourceTest, MutedSource) {
  // Setup the source for support a frame rate of 999 fps in order to test
  // the muted event faster. This is since the frame monitoring uses
  // PostDelayedTask that is dependent on the source frame rate.
  // Note that media::limits::kMaxFramesPerSecond is 1000.
  WebMediaStreamTrack track = CreateTrackAndStartSource(
      640, 480, media::limits::kMaxFramesPerSecond - 2);
  MockMediaStreamVideoSink sink;
  sink.ConnectToTrack(track);
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  bool muted_state = false;
  EXPECT_CALL(*mock_source(), DoSetMutedState(_))
      .WillOnce(DoAll(SaveArg<0>(&muted_state),
                      [&](auto) { std::move(quit_closure).Run(); }));
  run_loop.Run();
  EXPECT_EQ(muted_state, true);

  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateMuted);

  base::RunLoop run_loop2;
  base::OnceClosure quit_closure2 = run_loop2.QuitClosure();
  EXPECT_CALL(*mock_source(), DoSetMutedState(_))
      .WillOnce(DoAll(SaveArg<0>(&muted_state),
                      [&](auto) { std::move(quit_closure2).Run(); }));
  DeliverVideoFrameAndWaitForRenderer(640, 480, &sink);
  run_loop2.Run();

  EXPECT_EQ(muted_state, false);
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoSourceTest, ReconfigureTrack) {
  WebMediaStreamTrack track = CreateTrackAndStartSource(
      640, 480, media::limits::kMaxFramesPerSecond - 2);
  MockMediaStreamVideoSink sink;
  sink.ConnectToTrack(track);
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  MediaStreamVideoTrack* native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(settings.width, 640);
  EXPECT_EQ(settings.height, 480);
  EXPECT_EQ(settings.frame_rate, media::limits::kMaxFramesPerSecond - 2);
  EXPECT_EQ(settings.aspect_ratio, 640.0 / 480.0);

  source()->ReconfigureTrack(
      native_track, VideoTrackAdapterSettings(gfx::Size(630, 470), 30.0));
  native_track->GetSettings(settings);
  EXPECT_EQ(settings.width, 630);
  EXPECT_EQ(settings.height, 470);
  EXPECT_EQ(settings.frame_rate, 30.0);
  EXPECT_EQ(settings.aspect_ratio, 630.0 / 470.0);

  // Produce a frame in the source native format and expect the delivered frame
  // to have the new track format.
  DeliverVideoFrameAndWaitForRenderer(640, 480, &sink);
  EXPECT_EQ(1, sink.number_of_frames());
  EXPECT_EQ(630, sink.frame_size().width());
  EXPECT_EQ(470, sink.frame_size().height());
}

TEST_F(MediaStreamVideoSourceTest, ReconfigureStoppedTrack) {
  WebMediaStreamTrack track = CreateTrackAndStartSource(
      640, 480, media::limits::kMaxFramesPerSecond - 2);
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  MediaStreamVideoTrack* native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(settings.width, 640);
  EXPECT_EQ(settings.height, 480);
  EXPECT_EQ(settings.frame_rate, media::limits::kMaxFramesPerSecond - 2);
  EXPECT_EQ(settings.aspect_ratio, 640.0 / 480.0);

  // Reconfiguring a stopped track should have no effect since it is no longer
  // associated with the source.
  native_track->Stop();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateEnded);

  source()->ReconfigureTrack(
      native_track, VideoTrackAdapterSettings(gfx::Size(630, 470), 30.0));
  WebMediaStreamTrack::Settings stopped_settings;
  native_track->GetSettings(stopped_settings);
  EXPECT_EQ(stopped_settings.width, -1);
  EXPECT_EQ(stopped_settings.height, -1);
  EXPECT_EQ(stopped_settings.frame_rate, -1);
  EXPECT_EQ(stopped_settings.aspect_ratio, -1);
}

// Test that restart fails on a source without restart support.
TEST_F(MediaStreamVideoSourceTest, FailedRestart) {
  WebMediaStreamTrack track = CreateTrack("123");
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  // The source does not support Restart/StopForRestart.
  mock_source()->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_RUNNING);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  // Verify that Restart() fails with INVALID_STATE when not called after a
  // successful StopForRestart().
  mock_source()->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->StopSource();
  // Verify that StopForRestart() fails with INVALID_STATE when called when the
  // source is not running.
  mock_source()->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
}

// Test that restart succeeds on a source with restart support.
TEST_F(MediaStreamVideoSourceTest, SuccessfulRestart) {
  WebMediaStreamTrack track = CreateTrack("123");
  mock_source()->EnableStopForRestart();
  mock_source()->EnableRestart();
  mock_source()->StartMockedSource();
  EXPECT_EQ(NumberOfSuccessConstraintsCallbacks(), 1);
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  // Verify that StopForRestart() fails with INVALID_STATE called after the
  // source is already stopped.
  mock_source()->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_RUNNING);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  // Verify that Restart() fails with INVALID_STATE if the source has already
  // started.
  mock_source()->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->StopSource();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateEnded);
}

// Test that restart fails on a source without restart support.
TEST_F(MediaStreamVideoSourceTest, FailedRestartAfterStopForRestart) {
  WebMediaStreamTrack track = CreateTrack("123");
  mock_source()->EnableStopForRestart();
  mock_source()->DisableRestart();
  mock_source()->StartMockedSource();
  EXPECT_EQ(NumberOfSuccessConstraintsCallbacks(), 1);
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  // Another failed attempt to verify that the source remains in the correct
  // state.
  mock_source()->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  mock_source()->StopSource();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateEnded);
}

TEST_F(MediaStreamVideoSourceTest, StartStopAndNotifyRestartSupported) {
  WebMediaStreamTrack web_track = CreateTrack("123");
  mock_source()->EnableStopForRestart();
  mock_source()->StartMockedSource();
  EXPECT_EQ(NumberOfSuccessConstraintsCallbacks(), 1);
  EXPECT_EQ(web_track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  EXPECT_CALL(*this, MockNotification());
  WebPlatformMediaStreamTrack* track =
      WebPlatformMediaStreamTrack::GetTrack(web_track);
  track->StopAndNotify(WTF::Bind(&MediaStreamVideoSourceTest::MockNotification,
                                 base::Unretained(this)));
  EXPECT_EQ(web_track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateEnded);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamVideoSourceTest, StartStopAndNotifyRestartNotSupported) {
  WebMediaStreamTrack web_track = CreateTrack("123");
  mock_source()->DisableStopForRestart();
  mock_source()->StartMockedSource();
  EXPECT_EQ(NumberOfSuccessConstraintsCallbacks(), 1);
  EXPECT_EQ(web_track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);

  EXPECT_CALL(*this, MockNotification());
  WebPlatformMediaStreamTrack* track =
      WebPlatformMediaStreamTrack::GetTrack(web_track);
  track->StopAndNotify(WTF::Bind(&MediaStreamVideoSourceTest::MockNotification,
                                 base::Unretained(this)));
  EXPECT_EQ(web_track.Source().GetReadyState(),
            WebMediaStreamSource::kReadyStateEnded);
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamVideoSourceTest, StopSuspendedTrack) {
  WebMediaStreamTrack web_track1 = CreateTrack("123");
  mock_source()->StartMockedSource();
  WebMediaStreamTrack web_track2 = CreateTrack("123");

  // Simulate assigning |track1| to a sink, then removing it from the sink, and
  // then stopping it.
  MediaStreamVideoTrack* track1 =
      MediaStreamVideoTrack::GetVideoTrack(web_track1);
  mock_source()->UpdateHasConsumers(track1, true);
  mock_source()->UpdateHasConsumers(track1, false);
  track1->Stop();

  // Simulate assigning |track2| to a sink. The source should not be suspended.
  MediaStreamVideoTrack* track2 =
      MediaStreamVideoTrack::GetVideoTrack(web_track2);
  mock_source()->UpdateHasConsumers(track2, true);
  EXPECT_FALSE(mock_source()->is_suspended());
}

TEST_F(MediaStreamVideoSourceTest, AddTrackAfterStoppingSource) {
  WebMediaStreamTrack web_track1 = CreateTrack("123");
  mock_source()->StartMockedSource();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  EXPECT_EQ(0, NumberOfFailedConstraintsCallbacks());

  MediaStreamVideoTrack* track1 =
      MediaStreamVideoTrack::GetVideoTrack(web_track1);
  EXPECT_CALL(*this, MockNotification());
  // This is equivalent to track.stop() in JavaScript.
  track1->StopAndNotify(WTF::Bind(&MediaStreamVideoSourceTest::MockNotification,
                                  base::Unretained(this)));

  WebMediaStreamTrack track2 = CreateTrack("456");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());
  EXPECT_EQ(1, NumberOfFailedConstraintsCallbacks());
}

TEST_F(MediaStreamVideoSourceTest, AddsEncodedSinkWhenEncodedConsumerAppears) {
  EXPECT_CALL(*mock_source(), OnEncodedSinkEnabled).Times(1);
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled).Times(0);

  WebMediaStreamTrack track = CreateTrack();
  MockMediaStreamVideoSink sink;
  sink.ConnectEncodedToTrack(track);

  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoSourceTest, AddsEncodedSinkWhenEncodedConsumersAppear) {
  EXPECT_CALL(*mock_source(), OnEncodedSinkEnabled).Times(1);
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled).Times(0);

  WebMediaStreamTrack track1 = CreateTrack();
  MockMediaStreamVideoSink sink1;
  sink1.ConnectEncodedToTrack(track1);
  WebMediaStreamTrack track2 = CreateTrack();
  MockMediaStreamVideoSink sink2;
  sink2.ConnectEncodedToTrack(track2);

  Mock::VerifyAndClearExpectations(mock_source());
  sink1.DisconnectEncodedFromTrack();
  sink2.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoSourceTest,
       RemovesEncodedSinkWhenEncodedConsumerDisappears) {
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled).Times(1);
  WebMediaStreamTrack track = CreateTrack();
  MockMediaStreamVideoSink sink;
  sink.ConnectEncodedToTrack(track);
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoSourceTest,
       RemovesEncodedSinkWhenEncodedConsumersDisappear) {
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled).Times(1);
  WebMediaStreamTrack track1 = CreateTrack();
  MockMediaStreamVideoSink sink1;
  sink1.ConnectEncodedToTrack(track1);
  WebMediaStreamTrack track2 = CreateTrack();
  MockMediaStreamVideoSink sink2;
  sink2.ConnectEncodedToTrack(track2);
  sink1.DisconnectEncodedFromTrack();
  sink2.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoSourceTest, CapturingLinkSecureOnlyEncodedSinks) {
  InSequence s;
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  WebMediaStreamTrack track = CreateTrack();
  MockMediaStreamVideoSink sink;
  sink.ConnectEncodedToTrack(track);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoSourceTest, CapturingLinkSecureTracksAndEncodedSinks) {
  InSequence s;
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  WebMediaStreamTrack track = CreateTrack();
  mock_source()->UpdateCapturingLinkSecure(
      MediaStreamVideoTrack::GetVideoTrack(track), true);

  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  MockMediaStreamVideoSink sink;
  sink.ConnectEncodedToTrack(track);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink.DisconnectEncodedFromTrack();
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  mock_source()->UpdateCapturingLinkSecure(
      MediaStreamVideoTrack::GetVideoTrack(track), false);
}

}  // namespace blink
