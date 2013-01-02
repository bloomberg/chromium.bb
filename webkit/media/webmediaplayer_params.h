// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
#define WEBKIT_MEDIA_WEBMEDIAPLAYER_PARAMS_H_

#include "base/memory/ref_counted.h"
#include "media/filters/gpu_video_decoder.h"

namespace media {
class AudioRendererSink;
class MediaLog;
}

namespace WebKit {
class WebAudioSourceProvider;
}

namespace webkit_media {

class MediaStreamClient;

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class WebMediaPlayerParams {
 public:
  // |media_log| is the only required parameter; all others may be null.
  //
  // If provided, |audio_source_provider| and |audio_renderer_sink| arguments
  // must be the same object.
  //
  // TODO(scherkus): Remove WebAudioSourceProvider parameter once we
  // refactor RenderAudioSourceProvider to live under webkit/media/
  // instead of content/renderer/, see http://crbug.com/136442
  WebMediaPlayerParams(
      WebKit::WebAudioSourceProvider* audio_source_provider,
      const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink,
      const scoped_refptr<media::GpuVideoDecoder::Factories>& gpu_factories,
      MediaStreamClient* media_stream_client,
      const scoped_refptr<media::MediaLog>& media_log);
  ~WebMediaPlayerParams();

  WebKit::WebAudioSourceProvider* audio_source_provider() const {
    return audio_source_provider_;
  }

  const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink() const {
    return audio_renderer_sink_;
  }

  const scoped_refptr<media::GpuVideoDecoder::Factories>&
  gpu_factories() const {
    return gpu_factories_;
  }

  MediaStreamClient* media_stream_client() const {
    return media_stream_client_;
  }

  const scoped_refptr<media::MediaLog>& media_log() const {
    return media_log_;
  }

 private:
  WebKit::WebAudioSourceProvider* audio_source_provider_;
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink_;
  scoped_refptr<media::GpuVideoDecoder::Factories> gpu_factories_;
  MediaStreamClient* media_stream_client_;
  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
