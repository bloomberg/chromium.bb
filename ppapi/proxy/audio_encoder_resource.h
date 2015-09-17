// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_AUDIO_ENCODER_RESOURCE_H_
#define PPAPI_PROXY_AUDIO_ENCODER_RESOURCE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_encoder_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class SerializedHandle;
class VideoFrameResource;

class PPAPI_PROXY_EXPORT AudioEncoderResource
    : public PluginResource,
      public thunk::PPB_AudioEncoder_API {
 public:
  AudioEncoderResource(Connection connection, PP_Instance instance);
  ~AudioEncoderResource() override;

  thunk::PPB_AudioEncoder_API* AsPPB_AudioEncoder_API() override;

 private:
  // PPB_AduioEncoder_API implementation.
  int32_t GetSupportedProfiles(
      const PP_ArrayOutput& output,
      const scoped_refptr<TrackedCallback>& callback) override;
  int32_t Initialize(uint32_t channels,
                     PP_AudioBuffer_SampleRate input_sample_rate,
                     PP_AudioBuffer_SampleSize input_sample_size,
                     PP_AudioProfile output_profile,
                     uint32_t initial_bitrate,
                     PP_HardwareAcceleration acceleration,
                     const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetNumberOfSamples() override;
  int32_t GetBuffer(PP_Resource* audio_buffer,
                    const scoped_refptr<TrackedCallback>& callback) override;
  int32_t Encode(PP_Resource audio_buffer,
                 const scoped_refptr<TrackedCallback>& callback) override;
  int32_t GetBitstreamBuffer(
      PP_AudioBitstreamBuffer* bitstream_buffer,
      const scoped_refptr<TrackedCallback>& callback) override;
  void RecycleBitstreamBuffer(
      const PP_AudioBitstreamBuffer* bitstream_buffer) override;
  void RequestBitrateChange(uint32_t bitrate) override;
  void Close() override;

  DISALLOW_COPY_AND_ASSIGN(AudioEncoderResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_AUDIO_ENCODER_RESOURCE_H_
