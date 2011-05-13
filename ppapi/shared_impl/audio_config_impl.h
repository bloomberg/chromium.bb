// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_AUDIO_CONFIG_IMPL_H_
#define PPAPI_SHARED_IMPL_AUDIO_CONFIG_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource_object_base.h"
#include "ppapi/thunk/ppb_audio_config_api.h"

namespace ppapi {

class AudioConfigImpl : public thunk::PPB_AudioConfig_API {
 public:
  // You must call Init before using this object.
  AudioConfigImpl();
  virtual ~AudioConfigImpl();

  // Returns false if the arguments are invalid, the object should not be
  // used in this case.
  bool Init(PP_AudioSampleRate sample_rate, uint32_t sample_frame_count);

  // PPB_AudioConfig_API implementation.
  virtual PP_AudioSampleRate GetSampleRate() OVERRIDE;
  virtual uint32_t GetSampleFrameCount() OVERRIDE;

 private:
  PP_AudioSampleRate sample_rate_;
  uint32_t sample_frame_count_;

  DISALLOW_COPY_AND_ASSIGN(AudioConfigImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_AUDIO_CONFIG_IMPL_H_
