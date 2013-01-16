// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From trusted/ppb_buffer_trusted.idl modified Mon Jan 14 14:31:34 2013.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_buffer_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetSharedMemory(PP_Resource buffer, int* handle) {
  EnterResource<PPB_BufferTrusted_API> enter(buffer, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetSharedMemory(handle);
}

const PPB_BufferTrusted_0_1 g_ppb_buffertrusted_thunk_0_1 = {
  &GetSharedMemory
};

}  // namespace

const PPB_BufferTrusted_0_1* GetPPB_BufferTrusted_0_1_Thunk() {
  return &g_ppb_buffertrusted_thunk_0_1;
}

}  // namespace thunk
}  // namespace ppapi
