// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_audio_input_trusted_dev.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_input_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_AudioInput_API> EnterAudioInput;

PP_Resource Create(PP_Instance instance_id) {
  EnterResourceCreation enter(instance_id);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioInput(instance_id);
}

int32_t Open(PP_Resource audio_id,
             PP_Resource config_id,
             PP_CompletionCallback callback) {
  EnterAudioInput enter(audio_id, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->OpenTrusted("", config_id,
                                                     enter.callback()));
}

int32_t GetSyncSocket(PP_Resource audio_id, int* sync_socket) {
  EnterAudioInput enter(audio_id, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetSyncSocket(sync_socket);
}

int32_t GetSharedMemory(PP_Resource audio_id,
                        int* shm_handle,
                        uint32_t* shm_size) {
  EnterAudioInput enter(audio_id, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetSharedMemory(shm_handle, shm_size);
}

const PPB_AudioInputTrusted_Dev g_ppb_audioinput_trusted_thunk = {
  &Create,
  &Open,
  &GetSyncSocket,
  &GetSharedMemory,
};

}  // namespace

const PPB_AudioInputTrusted_Dev_0_1* GetPPB_AudioInputTrusted_0_1_Thunk() {
  return &g_ppb_audioinput_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
