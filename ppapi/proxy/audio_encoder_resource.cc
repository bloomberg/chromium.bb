// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_encoder_resource.h"

namespace ppapi {
namespace proxy {

AudioEncoderResource::AudioEncoderResource(Connection connection,
                                           PP_Instance instance)
    : PluginResource(connection, instance) {
}

AudioEncoderResource::~AudioEncoderResource() {
}

thunk::PPB_AudioEncoder_API* AudioEncoderResource::AsPPB_AudioEncoder_API() {
  return this;
}

int32_t AudioEncoderResource::GetSupportedProfiles(
    const PP_ArrayOutput& output,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t AudioEncoderResource::Initialize(
    uint32_t channels,
    PP_AudioBuffer_SampleRate input_sample_rate,
    PP_AudioBuffer_SampleSize input_sample_size,
    PP_AudioProfile output_profile,
    uint32_t initial_bitrate,
    PP_HardwareAcceleration acceleration,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t AudioEncoderResource::GetNumberOfSamples() {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t AudioEncoderResource::GetBuffer(
    PP_Resource* audio_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t AudioEncoderResource::Encode(
    PP_Resource audio_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

int32_t AudioEncoderResource::GetBitstreamBuffer(
    PP_AudioBitstreamBuffer* bitstream_buffer,
    const scoped_refptr<TrackedCallback>& callback) {
  return PP_ERROR_NOTSUPPORTED;
}

void AudioEncoderResource::RecycleBitstreamBuffer(
    const PP_AudioBitstreamBuffer* bitstream_buffer) {
}

void AudioEncoderResource::RequestBitrateChange(uint32_t bitrate) {
}

void AudioEncoderResource::Close() {
}

}  // namespace proxy
}  // namespace ppapi
