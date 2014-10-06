// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_AUDIO_BUFFER_RESOURCE_H_
#define PPAPI_PROXY_AUDIO_BUFFER_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_buffer_api.h"

namespace ppapi {

union MediaStreamBuffer;

namespace proxy {

class PPAPI_PROXY_EXPORT AudioBufferResource
    : public Resource,
      public thunk::PPB_AudioBuffer_API {
 public:
  AudioBufferResource(PP_Instance instance,
                     int32_t index,
                     MediaStreamBuffer* buffer);

  virtual ~AudioBufferResource();

  // PluginResource overrides:
  virtual thunk::PPB_AudioBuffer_API* AsPPB_AudioBuffer_API() override;

  // PPB_AudioBuffer_API overrides:
  virtual PP_TimeDelta GetTimestamp() override;
  virtual void SetTimestamp(PP_TimeDelta timestamp) override;
  virtual PP_AudioBuffer_SampleRate GetSampleRate() override;
  virtual PP_AudioBuffer_SampleSize GetSampleSize() override;
  virtual uint32_t GetNumberOfChannels() override;
  virtual uint32_t GetNumberOfSamples() override;
  virtual void* GetDataBuffer() override;
  virtual uint32_t GetDataBufferSize() override;
  virtual MediaStreamBuffer* GetBuffer() override;
  virtual int32_t GetBufferIndex() override;
  virtual void Invalidate() override;

  // Buffer index
  int32_t index_;

  MediaStreamBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(AudioBufferResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_AUDIO_BUFFER_RESOURCE_H_
