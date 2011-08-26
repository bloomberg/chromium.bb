// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

int32_t GetSharedMemory(PP_Resource resource,
                        int* handle,
                        uint32_t* byte_count) {
  EnterResource<PPB_ImageData_API> enter(resource, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  return enter.object()->GetSharedMemory(handle, byte_count);
}

const PPB_ImageDataTrusted g_ppb_image_data_trusted_thunk = {
  &GetSharedMemory
};

}  // namespace

const PPB_ImageDataTrusted* GetPPB_ImageDataTrusted_Thunk() {
  return &g_ppb_image_data_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
