// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_frame_resource.h"

#include "base/logging.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/media_stream_buffer.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

AudioFrameResource::AudioFrameResource(PP_Instance instance,
                                       int32_t index,
                                       MediaStreamBuffer* buffer)
    : Resource(OBJECT_IS_PROXY, instance),
      index_(index),
      buffer_(buffer) {
  DCHECK_EQ(buffer_->header.type, MediaStreamBuffer::TYPE_AUDIO);
}

AudioFrameResource::~AudioFrameResource() {
  CHECK(!buffer_) << "An unused (or unrecycled) frame is destroyed.";
}

thunk::PPB_AudioFrame_API* AudioFrameResource::AsPPB_AudioFrame_API() {
  return this;
}

PP_TimeDelta AudioFrameResource::GetTimestamp() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0.0;
  }
  return buffer_->audio.timestamp;
}

void AudioFrameResource::SetTimestamp(PP_TimeDelta timestamp) {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return;
  }
  buffer_->audio.timestamp = timestamp;
}

PP_AudioFrame_SampleRate AudioFrameResource::GetSampleRate() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return PP_AUDIOFRAME_SAMPLERATE_UNKNOWN;
  }
  return buffer_->audio.sample_rate;
}

PP_AudioFrame_SampleSize AudioFrameResource::GetSampleSize() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return PP_AUDIOFRAME_SAMPLESIZE_UNKNOWN;
  }
  return PP_AUDIOFRAME_SAMPLESIZE_16_BITS;
}

uint32_t AudioFrameResource::GetNumberOfChannels() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0;
  }
  return buffer_->audio.number_of_channels;
}

uint32_t AudioFrameResource::GetNumberOfSamples() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0;
  }
  return buffer_->audio.number_of_samples;
}

void* AudioFrameResource::GetDataBuffer() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return NULL;
  }
  return buffer_->audio.data;
}

uint32_t AudioFrameResource::GetDataBufferSize() {
  if (!buffer_) {
    VLOG(1) << "Buffer is invalid";
    return 0;
  }
  return buffer_->audio.data_size;
}

MediaStreamBuffer* AudioFrameResource::GetBuffer() {
  return buffer_;
}

int32_t AudioFrameResource::GetBufferIndex() {
  return index_;
}

void AudioFrameResource::Invalidate() {
  DCHECK(buffer_);
  DCHECK_GE(index_, 0);
  buffer_ = NULL;
  index_ = -1;
}

}  // namespace proxy
}  // namespace ppapi
