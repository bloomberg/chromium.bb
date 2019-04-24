// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_center.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "content/renderer/media/stream/webaudio_media_stream_source.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "media/base/sample_format.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_frame.h"

using blink::WebFrame;
using blink::WebView;

namespace content {

namespace {

void CreateNativeAudioMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebMediaStreamSource source = track.Source();
  blink::MediaStreamAudioSource* media_stream_source =
      blink::MediaStreamAudioSource::From(source);

  // At this point, a MediaStreamAudioSource instance must exist. The one
  // exception is when a WebAudio destination node is acting as a source of
  // audio.
  //
  // TODO(miu): This needs to be moved to an appropriate location. A WebAudio
  // source should have been created before this method was called so that this
  // special case code isn't needed here.
  if (!media_stream_source && source.RequiresAudioConsumer()) {
    DVLOG(1) << "Creating WebAudio media stream source.";
    media_stream_source = new WebAudioMediaStreamSource(&source);
    source.SetPlatformSource(
        base::WrapUnique(media_stream_source));  // Takes ownership.

    blink::WebMediaStreamSource::Capabilities capabilities;
    capabilities.device_id = source.Id();
    capabilities.echo_cancellation = std::vector<bool>({false});
    capabilities.auto_gain_control = std::vector<bool>({false});
    capabilities.noise_suppression = std::vector<bool>({false});
    capabilities.sample_size = {
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16),  // min
        media::SampleFormatToBitsPerChannel(media::kSampleFormatS16)   // max
    };
    source.SetCapabilities(capabilities);
  }

  if (media_stream_source)
    media_stream_source->ConnectToTrack(track);
  else
    LOG(DFATAL) << "WebMediaStreamSource missing its MediaStreamAudioSource.";
}

void CreateNativeVideoMediaStreamTrack(blink::WebMediaStreamTrack track) {
  DCHECK(track.GetPlatformTrack() == nullptr);
  blink::WebMediaStreamSource source = track.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  track.SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      native_source, MediaStreamVideoSource::ConstraintsCallback(),
      track.IsEnabled()));
}

void CloneNativeVideoMediaStreamTrack(
    const blink::WebMediaStreamTrack& original,
    blink::WebMediaStreamTrack clone) {
  DCHECK(!clone.GetPlatformTrack());
  blink::WebMediaStreamSource source = clone.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  MediaStreamVideoTrack* original_track =
      MediaStreamVideoTrack::GetVideoTrack(original);
  DCHECK(original_track);
  clone.SetPlatformTrack(std::make_unique<MediaStreamVideoTrack>(
      native_source, original_track->adapter_settings(),
      original_track->noise_reduction(), original_track->is_screencast(),
      original_track->min_frame_rate(),
      MediaStreamVideoSource::ConstraintsCallback(), clone.IsEnabled()));
}

}  // namespace

MediaStreamCenter::MediaStreamCenter() = default;

MediaStreamCenter::~MediaStreamCenter() = default;

void MediaStreamCenter::DidCreateMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::didCreateMediaStreamTrack";
  DCHECK(!track.IsNull() && !track.GetPlatformTrack());
  DCHECK(!track.Source().IsNull());

  switch (track.Source().GetType()) {
    case blink::WebMediaStreamSource::kTypeAudio:
      CreateNativeAudioMediaStreamTrack(track);
      break;
    case blink::WebMediaStreamSource::kTypeVideo:
      CreateNativeVideoMediaStreamTrack(track);
      break;
  }
}

void MediaStreamCenter::DidCloneMediaStreamTrack(
    const blink::WebMediaStreamTrack& original,
    const blink::WebMediaStreamTrack& clone) {
  DCHECK(!clone.IsNull());
  DCHECK(!clone.GetPlatformTrack());
  DCHECK(!clone.Source().IsNull());

  switch (clone.Source().GetType()) {
    case blink::WebMediaStreamSource::kTypeAudio:
      CreateNativeAudioMediaStreamTrack(clone);
      break;
    case blink::WebMediaStreamSource::kTypeVideo:
      CloneNativeVideoMediaStreamTrack(original, clone);
      break;
  }
}

void MediaStreamCenter::DidSetContentHint(
    const blink::WebMediaStreamTrack& track) {
  blink::WebPlatformMediaStreamTrack* native_track =
      blink::WebPlatformMediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetContentHint(track.ContentHint());
}

void MediaStreamCenter::DidEnableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebPlatformMediaStreamTrack* native_track =
      blink::WebPlatformMediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(true);
}

void MediaStreamCenter::DidDisableMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  blink::WebPlatformMediaStreamTrack* native_track =
      blink::WebPlatformMediaStreamTrack::GetTrack(track);
  if (native_track)
    native_track->SetEnabled(false);
}

blink::WebAudioSourceProvider*
MediaStreamCenter::CreateWebAudioSourceFromMediaStreamTrack(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "MediaStreamCenter::createWebAudioSourceFromMediaStreamTrack";
  blink::WebPlatformMediaStreamTrack* media_stream_track =
      track.GetPlatformTrack();
  if (!media_stream_track) {
    DLOG(ERROR) << "Native track missing for webaudio source.";
    return nullptr;
  }

  blink::WebMediaStreamSource source = track.Source();
  DCHECK_EQ(source.GetType(), blink::WebMediaStreamSource::kTypeAudio);

  // TODO(tommi): Rename WebRtcLocalAudioSourceProvider to
  // WebAudioMediaStreamSink since it's not specific to any particular source.
  // https://crbug.com/577874
  return new WebRtcLocalAudioSourceProvider(track);
}

void MediaStreamCenter::DidStopMediaStreamSource(
    const blink::WebMediaStreamSource& web_source) {
  if (web_source.IsNull())
    return;
  blink::WebPlatformMediaStreamSource* const source =
      web_source.GetPlatformSource();
  DCHECK(source);
  source->StopSource();
}

void MediaStreamCenter::GetSourceSettings(
    const blink::WebMediaStreamSource& web_source,
    blink::WebMediaStreamTrack::Settings& settings) {
  blink::MediaStreamAudioSource* const source =
      blink::MediaStreamAudioSource::From(web_source);
  if (!source)
    return;

  media::AudioParameters audio_parameters = source->GetAudioParameters();
  settings.sample_rate = audio_parameters.sample_rate();
  // kSampleFormatS16 is the format used for all audio input streams.
  settings.sample_size =
      media::SampleFormatToBitsPerChannel(media::kSampleFormatS16);
  settings.channel_count = audio_parameters.channels();
  settings.latency = audio_parameters.GetBufferDuration().InSecondsF();

  ProcessedLocalAudioSource* const processed_source =
      ProcessedLocalAudioSource::From(source);
  settings.volume = processed_source
                        ? static_cast<double>(processed_source->Volume()) /
                              processed_source->MaxVolume()
                        : 1.0;
}

}  // namespace content
