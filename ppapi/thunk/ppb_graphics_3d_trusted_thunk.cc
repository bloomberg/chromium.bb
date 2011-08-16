// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_graphics_3d_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_Graphics3D_API> EnterGraphics3D;

PP_Graphics3DTrustedState GetErrorState() {
  PP_Graphics3DTrustedState error_state = { 0 };
  error_state.error = PPB_GRAPHICS3D_TRUSTED_ERROR_GENERICERROR;
  return error_state;
}

PP_Resource CreateRaw(PP_Instance instance,
                      PP_Resource share_context,
                      const int32_t* attrib_list) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateGraphics3DRaw(
      instance, share_context, attrib_list);
}

PP_Bool InitCommandBuffer(PP_Resource context, int32_t size) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->InitCommandBuffer(size);
}

PP_Bool GetRingBuffer(PP_Resource context,
                      int* shm_handle,
                      uint32_t* shm_size) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetRingBuffer(shm_handle, shm_size);
}

PP_Graphics3DTrustedState GetState(PP_Resource context) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return GetErrorState();
  return enter.object()->GetState();
}

int32_t CreateTransferBuffer(PP_Resource context, uint32_t size) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->CreateTransferBuffer(size);
}

PP_Bool DestroyTransferBuffer(PP_Resource context, int32_t id) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->DestroyTransferBuffer(id);
}

PP_Bool GetTransferBuffer(PP_Resource context,
                          int32_t id,
                          int* shm_handle,
                          uint32_t* shm_size) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetTransferBuffer(id, shm_handle, shm_size);
}

PP_Bool Flush(PP_Resource context, int32_t put_offset) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Flush(put_offset);
}

PP_Graphics3DTrustedState FlushSync(PP_Resource context, int32_t put_offset) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return GetErrorState();
  return enter.object()->FlushSync(put_offset);
}

PP_Graphics3DTrustedState FlushSyncFast(PP_Resource context,
                                        int32_t put_offset,
                                        int32_t last_known_get) {
  EnterGraphics3D enter(context, true);
  if (enter.failed())
    return GetErrorState();
  return enter.object()->FlushSyncFast(put_offset, last_known_get);
}

const PPB_Graphics3DTrusted_Dev g_ppb_graphics_3d_trusted_thunk = {
  &CreateRaw,
  &InitCommandBuffer,
  &GetRingBuffer,
  &GetState,
  &CreateTransferBuffer,
  &DestroyTransferBuffer,
  &GetTransferBuffer,
  &Flush,
  &FlushSync,
  &FlushSyncFast,
};

}  // namespace

const PPB_Graphics3DTrusted_Dev* GetPPB_Graphics3DTrusted_Thunk() {
  return &g_ppb_graphics_3d_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi

