// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_

#include "content/renderer/media/audio/audio_device_factory.h"

namespace content {

class FuchsiaAudioDeviceFactory : public AudioDeviceFactory {
 public:
  FuchsiaAudioDeviceFactory();
  ~FuchsiaAudioDeviceFactory() final;

 protected:
  scoped_refptr<media::AudioRendererSink> CreateFinalAudioRendererSink(
      int render_frame_id,
      const media::AudioSinkParameters& params,
      base::TimeDelta auth_timeout) final;

  scoped_refptr<media::AudioRendererSink> CreateAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      int render_frame_id,
      const media::AudioSinkParameters& params) final;

  scoped_refptr<media::SwitchableAudioRendererSink>
  CreateSwitchableAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      int render_frame_id,
      const media::AudioSinkParameters& params) final;

  scoped_refptr<media::AudioCapturerSource> CreateAudioCapturerSource(
      int render_frame_id,
      const media::AudioSourceParameters& params) final;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_FUCHSIA_AUDIO_DEVICE_FACTORY_H_
