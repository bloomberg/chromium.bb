// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_trusted_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  EnterFunction<ResourceCreationAPI> enter(instance_id, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateAudioTrusted(instance_id);
}

int32_t Open(PP_Resource audio_id,
             PP_Resource config_id,
             PP_CompletionCallback created) {
  EnterResource<PPB_AudioTrusted_API> enter(audio_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->OpenTrusted(config_id, created);
}

int32_t GetSyncSocket(PP_Resource audio_id, int* sync_socket) {
  EnterResource<PPB_AudioTrusted_API> enter(audio_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetSyncSocket(sync_socket);
}

int32_t GetSharedMemory(PP_Resource audio_id,
                        int* shm_handle,
                        uint32_t* shm_size) {
  EnterResource<PPB_AudioTrusted_API> enter(audio_id, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetSharedMemory(shm_handle, shm_size);
}

const PPB_AudioTrusted g_ppb_audio_trusted_thunk = {
  &Create,
  &Open,
  &GetSyncSocket,
  &GetSharedMemory,
};

}  // namespace

const PPB_AudioTrusted* GetPPB_AudioTrusted_Thunk() {
  return &g_ppb_audio_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
