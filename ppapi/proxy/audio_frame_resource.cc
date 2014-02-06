// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_frame_resource.h"

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

AudioFrameResource::AudioFrameResource(PP_Instance instance,
                                       int32_t index,
                                       MediaStreamFrame* frame)
    : Resource(OBJECT_IS_PROXY, instance),
      index_(index),
      frame_(frame) {
  DCHECK_EQ(frame_->header.type, MediaStreamFrame::TYPE_AUDIO);
}

AudioFrameResource::~AudioFrameResource() {
  CHECK(!frame_) << "An unused (or unrecycled) frame is destroyed.";
}

thunk::PPB_AudioFrame_API* AudioFrameResource::AsPPB_AudioFrame_API() {
  return this;
}

PP_TimeDelta AudioFrameResource::GetTimestamp() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return 0.0;
  }
  return frame_->audio.timestamp;
}

void AudioFrameResource::SetTimestamp(PP_TimeDelta timestamp) {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return;
  }
  frame_->audio.timestamp = timestamp;
}

PP_AudioFrame_SampleRate AudioFrameResource::GetSampleRate() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return PP_AUDIOFRAME_SAMPLERATE_UNKNOWN;
  }
  return frame_->audio.sample_rate;
}

PP_AudioFrame_SampleSize AudioFrameResource::GetSampleSize() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return PP_AUDIOFRAME_SAMPLESIZE_UNKNOWN;
  }
  return PP_AUDIOFRAME_SAMPLESIZE_16_BITS;
}

uint32_t AudioFrameResource::GetNumberOfChannels() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return 0;
  }
  return frame_->audio.number_of_channels;
}

uint32_t AudioFrameResource::GetNumberOfSamples() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return 0;
  }
  return frame_->audio.number_of_samples;
}

void* AudioFrameResource::GetDataBuffer() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return NULL;
  }
  return frame_->audio.data;
}

uint32_t AudioFrameResource::GetDataBufferSize() {
  if (!frame_) {
    VLOG(1) << "Frame is invalid";
    return 0;
  }
  return frame_->audio.data_size;
}

MediaStreamFrame* AudioFrameResource::GetFrameBuffer() {
  return frame_;
}

int32_t AudioFrameResource::GetFrameBufferIndex() {
  return index_;
}

void AudioFrameResource::Invalidate() {
  DCHECK(frame_);
  DCHECK_GE(index_, 0);
  frame_ = NULL;
  index_ = -1;
}

}  // namespace proxy
}  // namespace ppapi
