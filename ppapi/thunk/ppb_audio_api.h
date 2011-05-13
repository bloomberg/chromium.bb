// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_AUDIO_API_H_
#define PPAPI_THUNK_AUDIO_API_H_

#include "ppapi/c/ppb_audio.h"

namespace ppapi {
namespace thunk {

class PPB_Audio_API {
 public:
  virtual PP_Resource GetCurrentConfig() = 0;
  virtual PP_Bool StartPlayback() = 0;
  virtual PP_Bool StopPlayback() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_AUDIO_API_H_
