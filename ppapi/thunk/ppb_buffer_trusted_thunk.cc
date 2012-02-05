// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetSharedMemory(PP_Resource buffer_id, int* shm_handle) {
  EnterResource<PPB_BufferTrusted_API> enter(buffer_id, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetSharedMemory(shm_handle);
}

const PPB_BufferTrusted g_ppb_buffer_trusted_thunk = {
  &GetSharedMemory,
};

}  // namespace

const PPB_BufferTrusted_0_1* GetPPB_BufferTrusted_0_1_Thunk() {
  return &g_ppb_buffer_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
