// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/mock_media_stream_registry.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/stream/mock_media_stream_video_source.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_source.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/modules/mediastream/video_track_adapter_settings.h"

namespace content {

namespace {

const char kTestStreamLabel[] = "stream_label";

class MockCDQualityAudioSource : public blink::MediaStreamAudioSource {
 public:
  MockCDQualityAudioSource()
      : blink::MediaStreamAudioSource(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            true) {
    SetFormat(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate,
        media::AudioParameters::kAudioCDSampleRate / 100));
    SetDevice(blink::MediaStreamDevice(
        blink::MEDIA_DEVICE_AUDIO_CAPTURE, "mock_audio_device_id",
        "Mock audio device", media::AudioParameters::kAudioCDSampleRate,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate / 100));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCDQualityAudioSource);
};

}  // namespace

MockMediaStreamRegistry::MockMediaStreamRegistry() {}

void MockMediaStreamRegistry::Init() {
  const blink::WebVector<blink::WebMediaStreamTrack> webkit_audio_tracks;
  const blink::WebVector<blink::WebMediaStreamTrack> webkit_video_tracks;
  const blink::WebString label(kTestStreamLabel);
  test_stream_.Initialize(label, webkit_audio_tracks, webkit_video_tracks);
}

void MockMediaStreamRegistry::AddVideoTrack(
    const std::string& track_id,
    const blink::VideoTrackAdapterSettings& adapter_settings,
    const base::Optional<bool>& noise_reduction,
    bool is_screencast,
    double min_frame_rate) {
  blink::WebMediaStreamSource blink_source;
  blink_source.Initialize("mock video source id",
                          blink::WebMediaStreamSource::kTypeVideo,
                          "mock video source name", false /* remote */);
  MockMediaStreamVideoSource* native_source = new MockMediaStreamVideoSource();
  blink_source.SetPlatformSource(base::WrapUnique(native_source));
  blink::WebMediaStreamTrack blink_track;
  blink_track.Initialize(blink::WebString::FromUTF8(track_id), blink_source);

  blink_track.SetPlatformTrack(std::make_unique<blink::MediaStreamVideoTrack>(
      native_source, adapter_settings, noise_reduction, is_screencast,
      min_frame_rate, blink::MediaStreamVideoSource::ConstraintsCallback(),
      true /* enabled */));
  test_stream_.AddTrack(blink_track);
}

void MockMediaStreamRegistry::AddVideoTrack(const std::string& track_id) {
  AddVideoTrack(track_id, blink::VideoTrackAdapterSettings(),
                base::Optional<bool>(), false /* is_screncast */,
                0.0 /* min_frame_rate */);
}

void MockMediaStreamRegistry::AddAudioTrack(const std::string& track_id) {
  blink::WebMediaStreamSource blink_source;
  blink_source.Initialize("mock audio source id",
                          blink::WebMediaStreamSource::kTypeAudio,
                          "mock audio source name", false /* remote */);
  blink::MediaStreamAudioSource* const source = new MockCDQualityAudioSource();
  blink_source.SetPlatformSource(base::WrapUnique(source));  // Takes ownership.

  blink::WebMediaStreamTrack blink_track;
  blink_track.Initialize(blink_source);
  CHECK(source->ConnectToTrack(blink_track));

  test_stream_.AddTrack(blink_track);
}

}  // namespace content
