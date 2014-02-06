// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_AUDIO_FRAME_RESOURCE_H_
#define PPAPI_PROXY_AUDIO_FRAME_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/media_stream_frame.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_frame_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT AudioFrameResource : public Resource,
                                              public thunk::PPB_AudioFrame_API {
 public:
  AudioFrameResource(PP_Instance instance,
                     int32_t index,
                     MediaStreamFrame* frame);

  virtual ~AudioFrameResource();

  // PluginResource overrides:
  virtual thunk::PPB_AudioFrame_API* AsPPB_AudioFrame_API() OVERRIDE;

  // PPB_AudioFrame_API overrides:
  virtual PP_TimeDelta GetTimestamp() OVERRIDE;
  virtual void SetTimestamp(PP_TimeDelta timestamp) OVERRIDE;
  virtual PP_AudioFrame_SampleRate GetSampleRate() OVERRIDE;
  virtual PP_AudioFrame_SampleSize GetSampleSize() OVERRIDE;
  virtual uint32_t GetNumberOfChannels() OVERRIDE;
  virtual uint32_t GetNumberOfSamples() OVERRIDE;
  virtual void* GetDataBuffer() OVERRIDE;
  virtual uint32_t GetDataBufferSize() OVERRIDE;
  virtual MediaStreamFrame* GetFrameBuffer();
  virtual int32_t GetFrameBufferIndex();
  virtual void Invalidate();

  // Frame index
  int32_t index_;

  MediaStreamFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(AudioFrameResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_AUDIO_FRAME_RESOURCE_H_
