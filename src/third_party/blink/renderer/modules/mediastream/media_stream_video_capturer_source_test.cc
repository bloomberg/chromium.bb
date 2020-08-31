// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using ::testing::_;
using ::testing::InSequence;

namespace blink {

namespace {

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() {}

  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD0(GetPreferredFormats, media::VideoCaptureFormats());
  MOCK_METHOD3(MockStartCapture,
               void(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback));
  MOCK_METHOD0(MockStopCapture, void());
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override {
    running_cb_ = running_callback;
    capture_params_ = params;
    MockStartCapture(params, new_frame_callback, running_callback);
    SetRunning(true);
  }
  void StopCapture() override { MockStopCapture(); }
  void SetRunning(bool is_running) {
    PostCrossThreadTask(*scheduler::GetSingleThreadTaskRunnerForTesting(),
                        FROM_HERE,
                        CrossThreadBindOnce(running_cb_, is_running));
  }
  const media::VideoCaptureParams& capture_params() const {
    return capture_params_;
  }

 private:
  RunningCallback running_cb_;
  media::VideoCaptureParams capture_params_;
};

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           base::OnceClosure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        got_frame_cb_(std::move(got_frame_cb)) {}

  void ConnectToTrack(const WebMediaStreamTrack& track) {
    MediaStreamVideoSink::ConnectToTrack(
        track,
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&FakeMediaStreamVideoSink::OnVideoFrame,
                                     WTF::CrossThreadUnretained(this))),
        true);
  }

  void DisconnectFromTrack() { MediaStreamVideoSink::DisconnectFromTrack(); }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks capture_time) {
    *capture_time_ = capture_time;
    metadata_->Clear();
    metadata_->MergeMetadataFrom(frame->metadata());
    std::move(got_frame_cb_).Run();
  }

 private:
  base::TimeTicks* const capture_time_;
  media::VideoFrameMetadata* const metadata_;
  base::OnceClosure got_frame_cb_;
};

}  // namespace

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest() : source_stopped_(false) {
    auto delegate = std::make_unique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, GetPreferredFormats());
    source_ = new MediaStreamVideoCapturerSource(
        /*LocalFrame =*/nullptr,
        WTF::Bind(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                  WTF::Unretained(this)),
        std::move(delegate));
    source_->SetMediaStreamDispatcherHostForTesting(
        mock_dispatcher_host_.CreatePendingRemoteAndBind());
    webkit_source_.Initialize(WebString::FromASCII("dummy_source_id"),
                              WebMediaStreamSource::kTypeVideo,
                              WebString::FromASCII("dummy_source_name"),
                              false /* remote */);
    webkit_source_.SetPlatformSource(base::WrapUnique(source_));
    webkit_source_id_ = webkit_source_.Id();

    MediaStreamVideoCapturerSource::DeviceCapturerFactoryCallback callback =
        WTF::BindRepeating(
            &MediaStreamVideoCapturerSourceTest::RecreateVideoCapturerSource,
            WTF::Unretained(this));
    source_->SetDeviceCapturerFactoryCallbackForTesting(std::move(callback));
  }

  void TearDown() override {
    webkit_source_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  WebMediaStreamTrack StartSource(
      const VideoTrackAdapterSettings& adapter_settings,
      const base::Optional<bool>& noise_reduction,
      bool is_screencast,
      double min_frame_rate) {
    bool enabled = true;
    // CreateVideoTrack will trigger OnConstraintsApplied.
    return MediaStreamVideoTrack::CreateVideoTrack(
        source_, adapter_settings, noise_reduction, is_screencast,
        min_frame_rate,
        WTF::Bind(&MediaStreamVideoCapturerSourceTest::OnConstraintsApplied,
                  base::Unretained(this)),
        enabled);
  }

  MockVideoCapturerSource& mock_delegate() { return *delegate_; }

  void OnSourceStopped(const WebMediaStreamSource& source) {
    source_stopped_ = true;
    if (source.IsNull())
      return;
    EXPECT_EQ(source.Id(), webkit_source_id_);
  }
  void OnStarted(bool result) {
    source_->OnRunStateChanged(delegate_->capture_params(), result);
  }

  void SetStopCaptureFlag() { stop_capture_flag_ = true; }

  MOCK_METHOD0(MockNotification, void());

  std::unique_ptr<media::VideoCapturerSource> RecreateVideoCapturerSource(
      const base::UnguessableToken& session_id) {
    auto delegate = std::make_unique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, MockStartCapture(_, _, _));
    return delegate;
  }

 protected:
  void OnConstraintsApplied(WebPlatformMediaStreamSource* source,
                            mojom::blink::MediaStreamRequestResult result,
                            const WebString& result_name) {}

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  WebMediaStreamSource webkit_source_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  MediaStreamVideoCapturerSource* source_;  // owned by |webkit_source_|.
  MockVideoCapturerSource* delegate_;       // owned by |source_|.
  WebString webkit_source_id_;
  bool source_stopped_;
  bool stop_capture_flag_ = false;
};

TEST_F(MediaStreamVideoCapturerSourceTest, StartAndStop) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), base::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // A bogus notification of running from the delegate when the source has
  // already started should not change the state.
  delegate_->SetRunning(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_TRUE(source_->GetCurrentCaptureParams().has_value());

  // If the delegate stops, the source should stop.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded,
            webkit_source_.GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
}

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTimeAndMetadataPlumbing) {
  VideoCaptureDeliverFrameCB deliver_frame_cb;
  media::VideoCapturerSource::RunningCallback running_cb;

  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<2>(&running_cb)));
  EXPECT_CALL(mock_delegate(), RequestRefreshFrame());
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), base::nullopt, false, 0.0);
  running_cb.Run(true);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time, &metadata,
      media::BindToCurrentLoop(run_loop.QuitClosure()));
  fake_sink.ConnectToTrack(track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE, 30.0);
  PostCrossThreadTask(
      *Platform::Current()->GetIOTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(deliver_frame_cb, frame, reference_capture_time));
  run_loop.Run();
  fake_sink.DisconnectFromTrack();
  EXPECT_EQ(reference_capture_time, capture_time);
  double metadata_value;
  EXPECT_TRUE(metadata.GetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                 &metadata_value));
  EXPECT_EQ(30.0, metadata_value);
}

TEST_F(MediaStreamVideoCapturerSourceTest, Restart) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), base::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);

  EXPECT_CALL(mock_delegate(), MockStopCapture());
  EXPECT_TRUE(source_->IsRunning());
  source_->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  // When the source has stopped for restart, the source is not considered
  // stopped, even if the underlying delegate is not running anymore.
  // WebPlatformMediaStreamSource::SourceStoppedCallback should not be
  // triggered.
  EXPECT_EQ(webkit_source_.GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);
  EXPECT_FALSE(source_stopped_);
  EXPECT_FALSE(source_->IsRunning());

  // A second StopForRestart() should fail with invalid state, since it only
  // makes sense when the source is running. Existing ready state should remain
  // the same.
  EXPECT_FALSE(source_->IsRunning());
  source_->StopForRestart(
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(webkit_source_.GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);
  EXPECT_FALSE(source_stopped_);
  EXPECT_FALSE(source_->IsRunning());

  // Restart the source. With the mock delegate, any video format will do.
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  EXPECT_FALSE(source_->IsRunning());
  source_->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_RUNNING);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(webkit_source_.GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);
  EXPECT_TRUE(source_->IsRunning());

  // A second Restart() should fail with invalid state since Restart() is
  // defined only when the source is stopped for restart. Existing ready state
  // should remain the same.
  EXPECT_TRUE(source_->IsRunning());
  source_->Restart(
      media::VideoCaptureFormat(),
      WTF::Bind([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(webkit_source_.GetReadyState(),
            WebMediaStreamSource::kReadyStateLive);
  EXPECT_TRUE(source_->IsRunning());

  // An delegate stop should stop the source and change the track state to
  // "ended".
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded,
            webkit_source_.GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
  EXPECT_FALSE(source_->IsRunning());
}

TEST_F(MediaStreamVideoCapturerSourceTest, StartStopAndNotify) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack web_track =
      StartSource(VideoTrackAdapterSettings(), base::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);

  stop_capture_flag_ = false;
  EXPECT_CALL(mock_delegate(), MockStopCapture())
      .WillOnce(InvokeWithoutArgs(
          this, &MediaStreamVideoCapturerSourceTest::SetStopCaptureFlag));
  EXPECT_CALL(*this, MockNotification());
  WebPlatformMediaStreamTrack* track =
      WebPlatformMediaStreamTrack::GetTrack(web_track);
  track->StopAndNotify(
      WTF::Bind(&MediaStreamVideoCapturerSourceTest::MockNotification,
                base::Unretained(this)));
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded,
            webkit_source_.GetReadyState());
  EXPECT_TRUE(source_stopped_);
  // It is a requirement that StopCapture() gets called in the same task as
  // StopAndNotify(), as CORS security checks for element capture rely on this.
  EXPECT_TRUE(stop_capture_flag_);
  // The readyState is updated in the current task, but the notification is
  // received on a separate task.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamVideoCapturerSourceTest, ChangeSource) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), base::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // A bogus notification of running from the delegate when the source has
  // already started should not change the state.
  delegate_->SetRunning(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // |ChangeSourceImpl()| will recreate the |delegate_|, so check the
  // |MockStartCapture()| invoking in the |RecreateVideoCapturerSource()|.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  MediaStreamDevice fake_video_device(
      mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, "Fake_Video_Device",
      "Fake Video Device");
  source_->ChangeSourceImpl(fake_video_device);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive,
            webkit_source_.GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // If the delegate stops, the source should stop.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded,
            webkit_source_.GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
}

}  // namespace blink
