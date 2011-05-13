// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_AUDIO_TRUSTED_API_H_
#define PPAPI_THUNK_AUDIO_TRUSTED_API_H_

#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/c/ppb_audio.h"

namespace ppapi {
namespace thunk {

class PPB_AudioTrusted_API {
 public:
  virtual int32_t OpenTrusted(PP_Resource config_id,
                              PP_CompletionCallback create_callback) = 0;
  virtual int32_t GetSyncSocket(int* sync_socket) = 0;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_AUDIO_TRUSTED_API_H_
