// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_flash_drm.idl modified Fri Aug  3 10:01:34 2018.

#include <stdint.h>

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_drm.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_flash_drm_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  VLOG(4) << "PPB_Flash_DRM::Create()";
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFlashDRM(instance);
}

int32_t GetDeviceID(PP_Resource drm,
                    struct PP_Var* id,
                    struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Flash_DRM::GetDeviceID()";
  EnterResource<PPB_Flash_DRM_API> enter(drm, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetDeviceID(id, enter.callback()));
}

PP_Bool GetHmonitor(PP_Resource drm, int64_t* hmonitor) {
  VLOG(4) << "PPB_Flash_DRM::GetHmonitor()";
  EnterResource<PPB_Flash_DRM_API> enter(drm, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->GetHmonitor(hmonitor);
}

int32_t GetVoucherFile(PP_Resource drm,
                       PP_Resource* file_ref,
                       struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Flash_DRM::GetVoucherFile()";
  EnterResource<PPB_Flash_DRM_API> enter(drm, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->GetVoucherFile(file_ref, enter.callback()));
}

int32_t MonitorIsExternal(PP_Resource drm,
                          PP_Bool* is_external,
                          struct PP_CompletionCallback callback) {
  VLOG(4) << "PPB_Flash_DRM::MonitorIsExternal()";
  EnterResource<PPB_Flash_DRM_API> enter(drm, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(
      enter.object()->MonitorIsExternal(is_external, enter.callback()));
}

const PPB_Flash_DRM_1_1 g_ppb_flash_drm_thunk_1_1 = {
    &Create, &GetDeviceID, &GetHmonitor, &GetVoucherFile, &MonitorIsExternal};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_Flash_DRM_1_1* GetPPB_Flash_DRM_1_1_Thunk() {
  return &g_ppb_flash_drm_thunk_1_1;
}

}  // namespace thunk
}  // namespace ppapi
