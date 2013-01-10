// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/webmediaplayer_params.h"

#include "media/base/audio_renderer_sink.h"
#include "media/base/media_log.h"

namespace webkit_media {

WebMediaPlayerParams::WebMediaPlayerParams(
    const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink,
    const scoped_refptr<media::GpuVideoDecoder::Factories>& gpu_factories,
    MediaStreamClient* media_stream_client,
    const scoped_refptr<media::MediaLog>& media_log)
    : audio_renderer_sink_(audio_renderer_sink),
      gpu_factories_(gpu_factories),
      media_stream_client_(media_stream_client),
      media_log_(media_log) {
  DCHECK(media_log_);
}

WebMediaPlayerParams::~WebMediaPlayerParams() {}

}  // namespace webkit_media
