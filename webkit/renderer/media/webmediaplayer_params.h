// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
#define WEBKIT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_

#include "base/memory/ref_counted.h"
#include "media/filters/gpu_video_decoder.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class AudioRendererSink;
class MediaLog;
}

namespace webkit_media {

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class WebMediaPlayerParams {
 public:
  // |message_loop_proxy| and |media_log| are the only required parameters;
  // all others may be null.
  WebMediaPlayerParams(
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink,
      const scoped_refptr<media::GpuVideoDecoder::Factories>& gpu_factories,
      const scoped_refptr<media::MediaLog>& media_log);
  ~WebMediaPlayerParams();

  const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy() const {
    return message_loop_proxy_;
  }

  const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink() const {
    return audio_renderer_sink_;
  }

  const scoped_refptr<media::GpuVideoDecoder::Factories>&
  gpu_factories() const {
    return gpu_factories_;
  }

  const scoped_refptr<media::MediaLog>& media_log() const {
    return media_log_;
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink_;
  scoped_refptr<media::GpuVideoDecoder::Factories> gpu_factories_;
  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // WEBKIT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
