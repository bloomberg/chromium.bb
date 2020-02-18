// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"

namespace blink {

namespace {

const char kTestStreamLabel[] = "stream_label";

class MockCDQualityAudioSource : public MediaStreamAudioSource {
 public:
  MockCDQualityAudioSource()
      : MediaStreamAudioSource(scheduler::GetSingleThreadTaskRunnerForTesting(),
                               true) {
    SetFormat(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate,
        media::AudioParameters::kAudioCDSampleRate / 100));
    SetDevice(MediaStreamDevice(
        mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, "mock_audio_device_id",
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
  const WebVector<WebMediaStreamTrack> webkit_audio_tracks;
  const WebVector<WebMediaStreamTrack> webkit_video_tracks;
  const WebString label(kTestStreamLabel);
  test_stream_.Initialize(label, webkit_audio_tracks, webkit_video_tracks);
}

MockMediaStreamVideoSource* MockMediaStreamRegistry::AddVideoTrack(
    const std::string& track_id,
    const VideoTrackAdapterSettings& adapter_settings,
    const base::Optional<bool>& noise_reduction,
    bool is_screencast,
    double min_frame_rate) {
  WebMediaStreamSource blink_source;
  blink_source.Initialize("mock video source id",
                          WebMediaStreamSource::kTypeVideo,
                          "mock video source name", false /* remote */);
  MockMediaStreamVideoSource* native_source = new MockMediaStreamVideoSource();
  blink_source.SetPlatformSource(base::WrapUnique(native_source));
  WebMediaStreamTrack blink_track;
  blink_track.Initialize(WebString::FromUTF8(track_id), blink_source);

  blink_track.SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      native_source, adapter_settings, noise_reduction, is_screencast,
      min_frame_rate, MediaStreamVideoSource::ConstraintsOnceCallback(),
      true /* enabled */));
  test_stream_.AddTrack(blink_track);
  return native_source;
}

MockMediaStreamVideoSource* MockMediaStreamRegistry::AddVideoTrack(
    const std::string& track_id) {
  return AddVideoTrack(track_id, VideoTrackAdapterSettings(),
                       base::Optional<bool>(), false /* is_screncast */,
                       0.0 /* min_frame_rate */);
}

void MockMediaStreamRegistry::AddAudioTrack(const std::string& track_id) {
  WebMediaStreamSource blink_source;
  blink_source.Initialize("mock audio source id",
                          WebMediaStreamSource::kTypeAudio,
                          "mock audio source name", false /* remote */);
  MediaStreamAudioSource* const source = new MockCDQualityAudioSource();
  blink_source.SetPlatformSource(base::WrapUnique(source));  // Takes ownership.

  WebMediaStreamTrack blink_track;
  blink_track.Initialize(blink_source);
  CHECK(source->ConnectToTrack(blink_track));

  test_stream_.AddTrack(blink_track);
}

}  // namespace blink
