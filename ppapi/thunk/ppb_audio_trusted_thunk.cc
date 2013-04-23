// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From trusted/ppb_audio_trusted.idl modified Mon Apr 22 12:01:08 2013.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource CreateTrusted(PP_Instance instance) {
  VLOG(4) << "PPB_AudioTrusted::CreateTrusted()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioTrusted(instance);
}

int32_t Open(PP_Resource audio,
             PP_Resource config,
             struct PP_CompletionCallback create_callback) {
  VLOG(4) << "PPB_AudioTrusted::Open()";
  EnterResource<PPB_Audio_API> enter(audio, create_callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open(config, enter.callback()));
}

int32_t GetSyncSocket(PP_Resource audio, int* sync_socket) {
  VLOG(4) << "PPB_AudioTrusted::GetSyncSocket()";
  EnterResource<PPB_Audio_API> enter(audio, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetSyncSocket(sync_socket);
}

int32_t GetSharedMemory(PP_Resource audio,
                        int* shm_handle,
                        uint32_t* shm_size) {
  VLOG(4) << "PPB_AudioTrusted::GetSharedMemory()";
  EnterResource<PPB_Audio_API> enter(audio, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetSharedMemory(shm_handle, shm_size);
}

const PPB_AudioTrusted_0_6 g_ppb_audiotrusted_thunk_0_6 = {
  &CreateTrusted,
  &Open,
  &GetSyncSocket,
  &GetSharedMemory
};

}  // namespace

const PPB_AudioTrusted_0_6* GetPPB_AudioTrusted_0_6_Thunk() {
  return &g_ppb_audiotrusted_thunk_0_6;
}

}  // namespace thunk
}  // namespace ppapi
