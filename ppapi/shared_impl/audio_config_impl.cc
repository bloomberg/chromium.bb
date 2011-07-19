// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/audio_config_impl.h"

namespace ppapi {

AudioConfigImpl::AudioConfigImpl()
    : sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {
}

AudioConfigImpl::~AudioConfigImpl() {
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

PP_AudioSampleRate AudioConfigImpl::GetSampleRate() {
  return sample_rate_;
}

uint32_t AudioConfigImpl::GetSampleFrameCount() {
  return sample_frame_count_;
}

}  // namespace ppapi
