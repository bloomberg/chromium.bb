// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_context_3d_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_Context3D_API> EnterContext3D;

PP_Context3DTrustedState GetErrorState() {
  PP_Context3DTrustedState error_state = { 0 };
  error_state.error = kGenericError;
  return error_state;
}

PP_Resource CreateRaw(PP_Instance instance,
                      PP_Config3D_Dev config,
                      PP_Resource share_context,
                      const int32_t* attrib_list) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateContext3DRaw(instance, config, share_context,
                                               attrib_list);
}

PP_Bool Initialize(PP_Resource context, int32_t size) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->InitializeTrusted(size);
}

PP_Bool GetRingBuffer(PP_Resource context,
                      int* shm_handle,
                      uint32_t* shm_size) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRingBuffer(shm_handle, shm_size);
}

PP_Context3DTrustedState GetState(PP_Resource context) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return GetErrorState();
  return enter.object()->GetState();
}

PP_Bool Flush(PP_Resource context, int32_t put_offset) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Flush(put_offset);
}

PP_Context3DTrustedState FlushSync(PP_Resource context, int32_t put_offset) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return GetErrorState();
  return enter.object()->FlushSync(put_offset);
}

int32_t CreateTransferBuffer(PP_Resource context, uint32_t size) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->CreateTransferBuffer(size);
}

PP_Bool DestroyTransferBuffer(PP_Resource context, int32_t id) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->DestroyTransferBuffer(id);
}

PP_Bool GetTransferBuffer(PP_Resource context,
                          int32_t id,
                          int* shm_handle,
                          uint32_t* shm_size) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetTransferBuffer(id, shm_handle, shm_size);
}

PP_Context3DTrustedState FlushSyncFast(PP_Resource context,
                                       int32_t put_offset,
                                       int32_t last_known_get) {
  EnterContext3D enter(context, true);
  if (enter.failed())
    return GetErrorState();
  return enter.object()->FlushSyncFast(put_offset, last_known_get);
}

const PPB_Context3DTrusted_Dev g_ppb_context_3d_trusted_thunk = {
  &CreateRaw,
  &Initialize,
  &GetRingBuffer,
  &GetState,
  &Flush,
  &FlushSync,
  &CreateTransferBuffer,
  &DestroyTransferBuffer,
  &GetTransferBuffer,
  &FlushSyncFast,
};

}  // namespace

const PPB_Context3DTrusted_Dev* GetPPB_Context3DTrusted_Thunk() {
  return &g_ppb_context_3d_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
