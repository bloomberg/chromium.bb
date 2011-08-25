// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/audio_config_impl.h"

namespace ppapi {

AudioConfigImpl::AudioConfigImpl(PP_Instance instance)
    : Resource(instance),
      sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

AudioConfigImpl::AudioConfigImpl(const HostResource& host_resource)
    : Resource(host_resource),
      sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

AudioConfigImpl::~AudioConfigImpl() {
}

// static
PP_Resource AudioConfigImpl::CreateAsImpl(PP_Instance instance,
                                          PP_AudioSampleRate sample_rate,
                                          uint32_t sample_frame_count) {
  scoped_refptr<AudioConfigImpl> object(new AudioConfigImpl(instance));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return object->GetReference();
}

// static
PP_Resource AudioConfigImpl::CreateAsProxy(PP_Instance instance,
                                           PP_AudioSampleRate sample_rate,
                                           uint32_t sample_frame_count) {
  scoped_refptr<AudioConfigImpl> object(new AudioConfigImpl(
      HostResource::MakeInstanceOnly(instance)));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return object->GetReference();
}

thunk::PPB_AudioConfig_API* AudioConfigImpl::AsPPB_AudioConfig_API() {
  return this;
}

PP_AudioSampleRate AudioConfigImpl::GetSampleRate() {
  return sample_rate_;
}

uint32_t AudioConfigImpl::GetSampleFrameCount() {
  return sample_frame_count_;
}

bool AudioConfigImpl::Init(PP_AudioSampleRate sample_rate,
                           uint32_t sample_frame_count) {
  // TODO(brettw): Currently we don't actually check what the hardware
  // supports, so just allow sample rates of the "guaranteed working" ones.
  if (sample_rate != PP_AUDIOSAMPLERATE_44100 &&
      sample_rate != PP_AUDIOSAMPLERATE_48000)
    return false;

  // TODO(brettw): Currently we don't actually query to get a value from the
  // hardware, so just validate the range.
  if (sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT ||
      sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return false;

  sample_rate_ = sample_rate;
  sample_frame_count_ = sample_frame_count;
  return true;
}

}  // namespace ppapi
