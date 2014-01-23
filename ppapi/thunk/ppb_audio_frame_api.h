// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_AUDIO_FRAME_API_H_
#define PPAPI_THUNK_PPB_AUDIO_FRAME_API_H_

#include "ppapi/c/ppb_audio_frame.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_AudioFrame_API {
 public:
  virtual ~PPB_AudioFrame_API() {}
  virtual PP_TimeDelta GetTimestamp() = 0;
  virtual void SetTimestamp(PP_TimeDelta timestamp) = 0;
  virtual PP_AudioFrame_SampleSize GetSampleSize() = 0;
  virtual uint32_t GetNumberOfChannels() = 0;
  virtual uint32_t GetNumberOfSamples() = 0;
  virtual void* GetDataBuffer() = 0;
  virtual uint32_t GetDataBufferSize() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_AUDIO_FRAME_API_H_
