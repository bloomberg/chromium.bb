// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_AUDIO_CONFIG_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_AUDIO_CONFIG_SHARED_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_audio_config_api.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_AudioConfig_Shared
    : public Resource,
      public thunk::PPB_AudioConfig_API {
 public:
  virtual ~PPB_AudioConfig_Shared();

  static PP_Resource CreateAsImpl(PP_Instance instance,
                                  PP_AudioSampleRate sample_rate,
                                  uint32_t sample_frame_count);
  static PP_Resource CreateAsProxy(PP_Instance instance,
                                   PP_AudioSampleRate sample_rate,
                                   uint32_t sample_frame_count);

  // Resource overrides.
  virtual thunk::PPB_AudioConfig_API* AsPPB_AudioConfig_API() OVERRIDE;

  // PPB_AudioConfig_API implementation.
  virtual PP_AudioSampleRate GetSampleRate() OVERRIDE;
  virtual uint32_t GetSampleFrameCount() OVERRIDE;

 private:
  // You must call Init before using this object.
  // Construct as implementation.
  explicit PPB_AudioConfig_Shared(PP_Instance instance);
  // Construct as proxy.
  explicit PPB_AudioConfig_Shared(const HostResource& host_resource);

  // Returns false if the arguments are invalid, the object should not be
  // used in this case.
  bool Init(PP_AudioSampleRate sample_rate, uint32_t sample_frame_count);

  PP_AudioSampleRate sample_rate_;
  uint32_t sample_frame_count_;

  DISALLOW_COPY_AND_ASSIGN(PPB_AudioConfig_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_AUDIO_CONFIG_SHARED_H_
