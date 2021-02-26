// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

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
  MediaStreamComponentVector audio_descriptions, video_descriptions;
  String label(kTestStreamLabel);
  descriptor_ = MakeGarbageCollected<MediaStreamDescriptor>(
      label, audio_descriptions, video_descriptions);
}

MockMediaStreamVideoSource* MockMediaStreamRegistry::AddVideoTrack(
    const std::string& track_id,
    const VideoTrackAdapterSettings& adapter_settings,
    const base::Optional<bool>& noise_reduction,
    bool is_screencast,
    double min_frame_rate) {
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "mock video source id", MediaStreamSource::kTypeVideo,
      "mock video source name", false /* remote */);
  auto native_source = std::make_unique<MockMediaStreamVideoSource>();
  auto* native_source_ptr = native_source.get();
  source->SetPlatformSource(std::move(native_source));

  auto* component = MakeGarbageCollected<MediaStreamComponent>(
      String::FromUTF8(track_id), source);
  component->SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      native_source_ptr, adapter_settings, noise_reduction, is_screencast,
      min_frame_rate, base::nullopt /* pan */, base::nullopt /* tilt */,
      base::nullopt /* zoom */, false /* pan_tilt_zoom_allowed */,
      MediaStreamVideoSource::ConstraintsOnceCallback(), true /* enabled */));
  descriptor_->AddRemoteTrack(component);
  return native_source_ptr;
}

MockMediaStreamVideoSource* MockMediaStreamRegistry::AddVideoTrack(
    const std::string& track_id) {
  return AddVideoTrack(track_id, VideoTrackAdapterSettings(),
                       base::Optional<bool>(), false /* is_screncast */,
                       0.0 /* min_frame_rate */);
}

void MockMediaStreamRegistry::AddAudioTrack(const std::string& track_id) {
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "mock audio source id", MediaStreamSource::kTypeAudio,
      "mock audio source name", false /* remote */);
  auto audio_source = std::make_unique<MockCDQualityAudioSource>();
  auto* audio_source_ptr = audio_source.get();
  source->SetPlatformSource(std::move(audio_source));

  auto* component = MakeGarbageCollected<MediaStreamComponent>(source);
  CHECK(audio_source_ptr->ConnectToTrack(component));

  descriptor_->AddRemoteTrack(component);
}

}  // namespace blink
