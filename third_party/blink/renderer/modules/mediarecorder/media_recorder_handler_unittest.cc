// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/scoped_mock_overlay_scrollbars.h"
#include "third_party/blink/renderer/modules/mediarecorder/fake_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"
#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_handler.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Lt;
using ::testing::Mock;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace blink {

static const std::string kTestVideoTrackId = "video_track_id";
static const std::string kTestAudioTrackId = "audio_track_id";
static const int kTestAudioChannels = 2;
static const int kTestAudioSampleRate = 48000;
static const int kTestAudioBufferDurationMs = 10;
// Opus works with 60ms buffers, so 6 MediaStreamAudioTrack Buffers are needed
// to encode one output buffer.
static const int kRatioOpusToTestAudioBuffers = 6;

struct MediaRecorderTestParams {
  const bool has_video;
  const bool has_audio;
  const char* const mime_type;
  const char* const codecs;
  const bool encoder_supports_alpha;
};

// Array of valid combinations of video/audio/codecs and expected collected
// encoded sizes to use for parameterizing MediaRecorderHandlerTest.
static const MediaRecorderTestParams kMediaRecorderTestParams[] = {
    {true, false, "video/webm", "vp8", true},
    {true, false, "video/webm", "vp9", true},
#if BUILDFLAG(RTC_USE_H264)
    {true, false, "video/x-matroska", "avc1", false},
#endif
    {false, true, "audio/webm", "opus", true},
    {false, true, "audio/webm", "", true},  // Should default to opus.
    {false, true, "audio/webm", "pcm", true},
    {true, true, "video/webm", "vp9,opus", true},
};

MediaStream* CreateMediaStream(V8TestingScope& scope) {
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "sourceId", MediaStreamSource::kTypeAudio, "sourceName", false);
  auto* component =
      MakeGarbageCollected<MediaStreamComponent>("audioTrack", source);

  auto* track = MakeGarbageCollected<MediaStreamTrack>(
      scope.GetExecutionContext(), component);

  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);

  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);

  return stream;
}

class MockMediaRecorder : public MediaRecorder {
 public:
  MockMediaRecorder(V8TestingScope& scope)
      : MediaRecorder(scope.GetExecutionContext(),
                      CreateMediaStream(scope),
                      MediaRecorderOptions::Create(),
                      scope.GetExceptionState()) {}
  virtual ~MockMediaRecorder() = default;

  MOCK_METHOD4(WriteData, void(const char*, size_t, bool, double));
  MOCK_METHOD1(OnError, void(const String& message));
};

class MediaRecorderHandlerTest : public TestWithParam<MediaRecorderTestParams>,
                                 public ScopedMockOverlayScrollbars {
 public:
  MediaRecorderHandlerTest()
      : media_recorder_handler_(MakeGarbageCollected<MediaRecorderHandler>(
            scheduler::GetSingleThreadTaskRunnerForTesting())),
        audio_source_(kTestAudioChannels,
                      440 /* freq */,
                      kTestAudioSampleRate) {
    EXPECT_FALSE(media_recorder_handler_->recording_);

    registry_.Init();
  }

  ~MediaRecorderHandlerTest() {
    registry_.reset();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  bool recording() const { return media_recorder_handler_->recording_; }
  bool hasVideoRecorders() const {
    return !media_recorder_handler_->video_recorders_.IsEmpty();
  }
  bool hasAudioRecorders() const {
    return !media_recorder_handler_->audio_recorders_.IsEmpty();
  }

  void OnVideoFrameForTesting(scoped_refptr<media::VideoFrame> frame) {
    media_recorder_handler_->OnVideoFrameForTesting(std::move(frame),
                                                    base::TimeTicks::Now());
  }

  void OnEncodedVideoForTesting(const media::WebmMuxer::VideoParameters& params,
                                std::string encoded_data,
                                std::string encoded_alpha,
                                base::TimeTicks timestamp,
                                bool is_key_frame) {
    media_recorder_handler_->OnEncodedVideo(params, encoded_data, encoded_alpha,
                                            timestamp, is_key_frame);
  }

  void OnAudioBusForTesting(const media::AudioBus& audio_bus) {
    media_recorder_handler_->OnAudioBusForTesting(audio_bus,
                                                  base::TimeTicks::Now());
  }
  void SetAudioFormatForTesting(const media::AudioParameters& params) {
    media_recorder_handler_->SetAudioFormatForTesting(params);
  }

  void AddVideoTrack() {
    video_source_ = registry_.AddVideoTrack(kTestVideoTrackId);
  }

  void AddTracks() {
    // Avoid issues with non-parameterized tests by calling this outside of ctr.
    if (GetParam().has_video)
      AddVideoTrack();
    if (GetParam().has_audio)
      registry_.AddAudioTrack(kTestAudioTrackId);
  }

  void ForceOneErrorInWebmMuxer() {
    media_recorder_handler_->webm_muxer_->ForceOneLibWebmErrorForTesting();
  }

  std::unique_ptr<media::AudioBus> NextAudioBus() {
    std::unique_ptr<media::AudioBus> bus(media::AudioBus::Create(
        kTestAudioChannels,
        kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000));
    audio_source_.OnMoreData(base::TimeDelta(), base::TimeTicks::Now(), 0,
                             bus.get());
    return bus;
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  MockMediaStreamRegistry registry_;

  // The Class under test. Needs to be scoped_ptr to force its destruction.
  Persistent<MediaRecorderHandler> media_recorder_handler_;

  // For generating test AudioBuses
  media::SineWaveAudioSource audio_source_;

  MockMediaStreamVideoSource* video_source_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRecorderHandlerTest);
};

// Checks that canSupportMimeType() works as expected, by sending supported
// combinations and unsupported ones.
TEST_F(MediaRecorderHandlerTest, CanSupportMimeType) {
  const String unsupported_mime_type("video/mpeg");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      unsupported_mime_type, String()));

  const String mime_type_video("video/webm");
  EXPECT_TRUE(
      media_recorder_handler_->CanSupportMimeType(mime_type_video, String()));
  const String mime_type_video_uppercase("video/WEBM");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video_uppercase, String()));
  const String example_good_codecs_1("vp8");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_1));
  const String example_good_codecs_2("vp9,opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_2));
  const String example_good_codecs_3("VP9,opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_3));
  const String example_good_codecs_4("H264");
#if BUILDFLAG(RTC_USE_H264)
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_4));
#else
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_good_codecs_4));
#endif

  const String example_unsupported_codecs_1("daala");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_video, example_unsupported_codecs_1));

  const String mime_type_audio("audio/webm");
  EXPECT_TRUE(
      media_recorder_handler_->CanSupportMimeType(mime_type_audio, String()));
  const String example_good_codecs_5("opus");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_good_codecs_5));
  const String example_good_codecs_6("OpUs");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_good_codecs_6));
  const String example_good_codecs_7("pcm");
  EXPECT_TRUE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_good_codecs_7));

  const String example_unsupported_codecs_2("vorbis");
  EXPECT_FALSE(media_recorder_handler_->CanSupportMimeType(
      mime_type_audio, example_unsupported_codecs_2));
}

// Checks that the initialization-destruction sequence works fine.
TEST_P(MediaRecorderHandlerTest, InitializeStartStop) {
  AddTracks();
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));
  EXPECT_FALSE(recording());
  EXPECT_FALSE(hasVideoRecorders());
  EXPECT_FALSE(hasAudioRecorders());

  EXPECT_TRUE(media_recorder_handler_->Start(0));
  EXPECT_TRUE(recording());

  EXPECT_TRUE(hasVideoRecorders() || !GetParam().has_video);
  EXPECT_TRUE(hasAudioRecorders() || !GetParam().has_audio);

  media_recorder_handler_->Stop();
  EXPECT_FALSE(recording());
  EXPECT_FALSE(hasVideoRecorders());
  EXPECT_FALSE(hasAudioRecorders());

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

// Sends 2 opaque frames and 1 transparent frame and expects them as WebM
// contained encoded data in writeData().
TEST_P(MediaRecorderHandlerTest, EncodeVideoFrames) {
  // Video-only test.
  if (GetParam().has_audio)
    return;

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));
  EXPECT_TRUE(media_recorder_handler_->Start(0));

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  {
    const size_t kEncodedSizeThreshold = 16;
    base::RunLoop run_loop;
    // writeData() is pinged a number of times as the WebM header is written;
    // the last time it is called it has the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(this);

  {
    const size_t kEncodedSizeThreshold = 12;
    base::RunLoop run_loop;
    // The second time around writeData() is called a number of times to write
    // the WebM frame header, and then is pinged with the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(this);

  {
    const scoped_refptr<media::VideoFrame> alpha_frame =
        media::VideoFrame::CreateTransparentFrame(gfx::Size(160, 80));
    const size_t kEncodedSizeThreshold = 16;
    EXPECT_EQ(4u, media::VideoFrame::NumPlanes(alpha_frame->format()));
    base::RunLoop run_loop;
    // The second time around writeData() is called a number of times to write
    // the WebM frame header, and then is pinged with the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    if (GetParam().encoder_supports_alpha) {
      EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _))
          .Times(AtLeast(1));
      EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
          .Times(1)
          .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    }

    OnVideoFrameForTesting(alpha_frame);
    run_loop.Run();
  }

  media_recorder_handler_->Stop();

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerTest,
                         ValuesIn(kMediaRecorderTestParams));

// Sends 2 frames and expect them as WebM (or MKV) contained encoded audio data
// in writeData().
TEST_P(MediaRecorderHandlerTest, OpusEncodeAudioFrames) {
  // Audio-only test.
  if (GetParam().has_video)
    return;

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));
  EXPECT_TRUE(media_recorder_handler_->Start(0));

  InSequence s;
  const std::unique_ptr<media::AudioBus> audio_bus1 = NextAudioBus();
  const std::unique_ptr<media::AudioBus> audio_bus2 = NextAudioBus();

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LINEAR, media::CHANNEL_LAYOUT_STEREO,
      kTestAudioSampleRate,
      kTestAudioSampleRate * kTestAudioBufferDurationMs / 1000);
  SetAudioFormatForTesting(params);

  const size_t kEncodedSizeThreshold = 24;
  {
    base::RunLoop run_loop;
    // writeData() is pinged a number of times as the WebM header is written;
    // the last time it is called it has the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    for (int i = 0; i < kRatioOpusToTestAudioBuffers; ++i)
      OnAudioBusForTesting(*audio_bus1);
    run_loop.Run();
  }
  Mock::VerifyAndClearExpectations(this);

  {
    base::RunLoop run_loop;
    // The second time around writeData() is called a number of times to write
    // the WebM frame header, and then is pinged with the encoded data.
    EXPECT_CALL(*recorder, WriteData(_, Lt(kEncodedSizeThreshold), _, _))
        .Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    for (int i = 0; i < kRatioOpusToTestAudioBuffers; ++i)
      OnAudioBusForTesting(*audio_bus2);
    run_loop.Run();
  }

  media_recorder_handler_->Stop();

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

// Starts up recording and forces a WebmMuxer's libwebm error.
TEST_P(MediaRecorderHandlerTest, WebmMuxerErrorWhileEncoding) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio)
    return;

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));
  EXPECT_TRUE(media_recorder_handler_->Start(0));

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  {
    const size_t kEncodedSizeThreshold = 16;
    base::RunLoop run_loop;
    EXPECT_CALL(*recorder, WriteData(_, _, _, _)).Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Gt(kEncodedSizeThreshold), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }

  ForceOneErrorInWebmMuxer();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*recorder, WriteData(_, _, _, _)).Times(0);
    EXPECT_CALL(*recorder, OnError(_))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

// Checks the ActualMimeType() versus the expected.
TEST_P(MediaRecorderHandlerTest, ActualMimeType) {
  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));

  StringBuilder actual_mime_type;
  actual_mime_type.Append(GetParam().mime_type);
  actual_mime_type.Append(";codecs=");
  if (strlen(GetParam().codecs) != 0u)
    actual_mime_type.Append(GetParam().codecs);
  else if (GetParam().has_video)
    actual_mime_type.Append("vp8");
  else if (GetParam().has_audio)
    actual_mime_type.Append("opus");

  EXPECT_EQ(media_recorder_handler_->ActualMimeType(),
            actual_mime_type.ToString());

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

TEST_P(MediaRecorderHandlerTest, PauseRecorderForVideo) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio)
    return;

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));
  EXPECT_TRUE(media_recorder_handler_->Start(0));

  Mock::VerifyAndClearExpectations(recorder);
  media_recorder_handler_->Pause();

  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media::WebmMuxer::VideoParameters params(gfx::Size(), 1, media::kCodecVP9,
                                           gfx::ColorSpace());
  OnEncodedVideoForTesting(params, "vp9 frame", "", base::TimeTicks::Now(),
                           true);

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

TEST_P(MediaRecorderHandlerTest, StartStopStartRecorderForVideo) {
  // Video-only test: Audio would be very similar.
  if (GetParam().has_audio)
    return;

  AddTracks();

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);

  const String mime_type(GetParam().mime_type);
  const String codecs(GetParam().codecs);

  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), mime_type, codecs, 0, 0));
  EXPECT_TRUE(media_recorder_handler_->Start(0));
  media_recorder_handler_->Stop();

  Mock::VerifyAndClearExpectations(recorder);
  EXPECT_TRUE(media_recorder_handler_->Start(0));

  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));
  media::WebmMuxer::VideoParameters params(gfx::Size(), 1, media::kCodecVP9,
                                           gfx::ColorSpace());
  OnEncodedVideoForTesting(params, "vp9 frame", "", base::TimeTicks::Now(),
                           true);

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

struct MediaRecorderPassthroughTestParams {
  const char* mime_type;
  media::VideoCodec codec;
};

static const MediaRecorderPassthroughTestParams
    kMediaRecorderPassthroughTestParams[] = {
        {"video/webm;codecs=vp8", media::kCodecVP8},
        {"video/webm;codecs=vp9", media::kCodecVP9},
#if BUILDFLAG(RTC_USE_H264)
        {"video/x-matroska;codecs=avc1", media::kCodecH264},
#endif
};

class MediaRecorderHandlerPassthroughTest
    : public TestWithParam<MediaRecorderPassthroughTestParams>,
      public ScopedMockOverlayScrollbars {
 public:
  MediaRecorderHandlerPassthroughTest() {
    registry_.Init();
    video_source_ = registry_.AddVideoTrack(kTestVideoTrackId);
    ON_CALL(*video_source_, SupportsEncodedOutput).WillByDefault(Return(true));
    media_recorder_handler_ = MakeGarbageCollected<MediaRecorderHandler>(
        scheduler::GetSingleThreadTaskRunnerForTesting());
    EXPECT_FALSE(media_recorder_handler_->recording_);
  }

  ~MediaRecorderHandlerPassthroughTest() {
    registry_.reset();
    media_recorder_handler_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  void OnVideoFrameForTesting(scoped_refptr<EncodedVideoFrame> frame) {
    media_recorder_handler_->OnEncodedVideoFrameForTesting(
        std::move(frame), base::TimeTicks::Now());
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  MockMediaStreamRegistry registry_;
  MockMediaStreamVideoSource* video_source_ = nullptr;
  Persistent<MediaRecorderHandler> media_recorder_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRecorderHandlerPassthroughTest);
};

TEST_P(MediaRecorderHandlerPassthroughTest, PassesThrough) {
  // Setup the mock video source to allow for passthrough recording.
  EXPECT_CALL(*video_source_, OnEncodedSinkEnabled);
  EXPECT_CALL(*video_source_, OnEncodedSinkDisabled);

  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  media_recorder_handler_->Initialize(recorder, registry_.test_stream(), "", "",
                                      0, 0);
  media_recorder_handler_->Start(0);

  const size_t kFrameSize = 42;
  auto frame = FakeEncodedVideoFrame::Builder()
                   .WithKeyFrame(true)
                   .WithCodec(GetParam().codec)
                   .WithData(std::string(kFrameSize, 'P'))
                   .BuildRefPtr();
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*recorder, WriteData(_, _, _, _)).Times(AtLeast(1));
    EXPECT_CALL(*recorder, WriteData(_, Ge(kFrameSize), _, _))
        .Times(1)
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    OnVideoFrameForTesting(frame);
    run_loop.Run();
  }
  EXPECT_EQ(media_recorder_handler_->ActualMimeType(), GetParam().mime_type);
  Mock::VerifyAndClearExpectations(this);

  media_recorder_handler_->Stop();

  // Expect a last call on destruction.
  EXPECT_CALL(*recorder, WriteData(_, _, true, _)).Times(1);
  media_recorder_handler_ = nullptr;
}

TEST_F(MediaRecorderHandlerPassthroughTest, ErrorsOutOnCodecSwitch) {
  V8TestingScope scope;
  auto* recorder = MakeGarbageCollected<MockMediaRecorder>(scope);
  EXPECT_TRUE(media_recorder_handler_->Initialize(
      recorder, registry_.test_stream(), "", "", 0, 0));
  EXPECT_TRUE(media_recorder_handler_->Start(0));

  // NOTE, Asan: the prototype of WriteData which has a const char* as data
  // ptr plays badly with gmock which tries to interpret it as a null-terminated
  // string. However, it points to binary data which causes gmock to overrun the
  // bounds of buffers and this manifests as an ASAN crash.
  // The expectation here works around this issue.
  EXPECT_CALL(*recorder, WriteData).Times(AtLeast(1));

  EXPECT_CALL(*recorder, OnError).WillOnce(Invoke([&](const String&) {
    // Simulate MediaRecorder behavior which is to Stop() the handler on error.
    media_recorder_handler_->Stop();
  }));
  OnVideoFrameForTesting(FakeEncodedVideoFrame::Builder()
                             .WithKeyFrame(true)
                             .WithCodec(media::kCodecVP8)
                             .WithData(std::string("vp8 frame"))
                             .BuildRefPtr());
  // Switch to VP9 frames. This is expected to cause the call to OnError
  // above.
  OnVideoFrameForTesting(FakeEncodedVideoFrame::Builder()
                             .WithKeyFrame(true)
                             .WithCodec(media::kCodecVP9)
                             .WithData(std::string("vp9 frame"))
                             .BuildRefPtr());
  // Send one more frame to verify that continued frame of different codec
  // transfer doesn't crash the media recorder.
  OnVideoFrameForTesting(FakeEncodedVideoFrame::Builder()
                             .WithKeyFrame(true)
                             .WithCodec(media::kCodecVP8)
                             .WithData(std::string("vp8 frame"))
                             .BuildRefPtr());
  platform_->RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaRecorderHandlerPassthroughTest,
                         ValuesIn(kMediaRecorderPassthroughTestParams));

}  // namespace blink
