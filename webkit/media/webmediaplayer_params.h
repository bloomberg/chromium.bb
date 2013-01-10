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

namespace webkit_media {

class MediaStreamClient;

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class WebMediaPlayerParams {
 public:
  // |media_log| is the only required parameter; all others may be null.
  WebMediaPlayerParams(
      const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink,
      const scoped_refptr<media::GpuVideoDecoder::Factories>& gpu_factories,
      MediaStreamClient* media_stream_client,
      const scoped_refptr<media::MediaLog>& media_log);
  ~WebMediaPlayerParams();

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
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink_;
  scoped_refptr<media::GpuVideoDecoder::Factories> gpu_factories_;
  MediaStreamClient* media_stream_client_;
  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // WEBKIT_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
