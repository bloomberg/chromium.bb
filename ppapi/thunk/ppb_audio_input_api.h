// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_AUDIO_INPUT_API_H_
#define PPAPI_THUNK_AUDIO_INPUT_API_H_

#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

class PPB_AudioInput_API {
 public:
  virtual ~PPB_AudioInput_API() {}

  virtual PP_Resource GetCurrentConfig() = 0;
  virtual PP_Bool StartCapture() = 0;
  virtual PP_Bool StopCapture() = 0;

  // Trusted API.
  virtual int32_t OpenTrusted(PP_Resource config_id,
                              PP_CompletionCallback create_callback) = 0;
  virtual int32_t GetSyncSocket(int* sync_socket) = 0;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_AUDIO_INPUT_API_H_
